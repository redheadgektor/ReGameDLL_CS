/*
*
*   This program is free software; you can redistribute it and/or modify it
*   under the terms of the GNU General Public License as published by the
*   Free Software Foundation; either version 2 of the License, or (at
*   your option) any later version.
*
*   This program is distributed in the hope that it will be useful, but
*   WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*   General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software Foundation,
*   Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*   In addition, as a special exception, the author gives permission to
*   link the code of this program with the Half-Life Game Engine ("HL
*   Engine") and Modified Game Libraries ("MODs") developed by Valve,
*   L.L.C ("Valve").  You must obey the GNU General Public License in all
*   respects for all of the code used other than the HL Engine and MODs
*   from Valve.  If you modify this file, you may extend this exception
*   to your version of the file, but you are not obligated to do so.  If
*   you do not wish to do so, delete this exception statement from your
*   version.
*
*/

#include "precompiled.h"

// Begin the hunt
void HuntState::OnEnter(CCSBot *me)
{
	// lurking death
	if (me->IsUsingKnife() && me->IsWellPastSafe() && !me->IsHurrying())
		me->Walk();
	else
		me->Run();

	me->StandUp();
	me->SetDisposition(CCSBot::ENGAGE_AND_INVESTIGATE);
	me->SetTask(CCSBot::SEEK_AND_DESTROY);
	me->DestroyPath();
}

// Hunt down our enemies
void HuntState::OnUpdate(CCSBot *me)
{
	// if we've been hunting for a long time, drop into Idle for a moment to
	// select something else to do
	const float huntingTooLongTime = 30.0f;
	if (gpGlobals->time - me->GetStateTimestamp() > huntingTooLongTime)
	{
		// stop being a rogue and do the scenario, since there must not be many enemies left to hunt
		me->PrintIfWatched("Giving up hunting, and being a rogue\n");
		me->SetRogue(false);
		me->Idle();
		return;
	}

	// scenario logic
	if (TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_DEFUSE_BOMB)
	{
		if (me->m_iTeam == TERRORIST)
		{
			// if we have the bomb and it's time to plant, or we happen to be in a bombsite and it seems safe, do it
			if (me->IsCarryingBomb())
			{
				const float safeTime = 3.0f;
				if (TheCSBots()->IsTimeToPlantBomb() || (me->IsAtBombsite() && gpGlobals->time - me->GetLastSawEnemyTimestamp() > safeTime))
				{
					me->Idle();
					return;
				}
			}

			// if we notice the bomb lying on the ground, go get it
			if (me->NoticeLooseBomb())
			{
				me->FetchBomb();
				return;
			}

			// if bomb has been planted, and we hear it, move to a hiding spot near the bomb and watch it
			const Vector *bombPos = me->GetGameState()->GetBombPosition();
			if (!me->IsRogue() && me->GetGameState()->IsBombPlanted() && bombPos)
			{
				me->SetTask(CCSBot::GUARD_TICKING_BOMB);
				me->Hide(TheNavAreaGrid.GetNavArea(bombPos));
				return;
			}
		}
		// CT
		else
		{
			if (!me->IsRogue() && me->CanSeeLooseBomb())
			{
				CNavArea *looseBombArea = TheCSBots()->GetLooseBombArea();

				// if we are near the loose bomb and can see it, hide nearby and guard it
				me->SetTask(CCSBot::GUARD_LOOSE_BOMB);
				me->Hide(looseBombArea);

				if (looseBombArea)
					me->GetChatter()->AnnouncePlan("GoingToGuardLooseBomb", looseBombArea->GetPlace());

				return;
			}
			else if (TheCSBots()->IsBombPlanted())
			{
				// rogues will defuse a bomb, but not guard the defuser
				if (!me->IsRogue() || !TheCSBots()->GetBombDefuser())
				{
					// search for the planted bomb to defuse
					me->Idle();
					return;
				}
			}
		}
	}
	else if (TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_RESCUE_HOSTAGES)
	{
		if (me->m_iTeam == TERRORIST)
		{
			if (me->GetGameState()->AreAllHostagesBeingRescued())
			{
				// all hostages are being rescued, head them off at the escape zones
				if (me->GuardRandomZone())
				{
					me->SetTask(CCSBot::GUARD_HOSTAGE_RESCUE_ZONE);
					me->PrintIfWatched("Trying to beat them to an escape zone!\n");
					me->SetDisposition(CCSBot::OPPORTUNITY_FIRE);
					me->GetChatter()->GuardingHostageEscapeZone(IS_PLAN);
					return;
				}
			}

			// if safe time is up, and we stumble across a hostage, guard it
			if (!me->IsRogue() && !me->IsSafe())
			{
				CHostage *pHostage = me->GetGameState()->GetNearestVisibleFreeHostage();
				if (pHostage)
				{
					CNavArea *area = TheNavAreaGrid.GetNearestNavArea(&pHostage->pev->origin);
					if (area)
					{
						// we see a free hostage, guard it
						me->SetTask(CCSBot::GUARD_HOSTAGES);
						me->Hide(area);
						me->PrintIfWatched("I'm guarding hostages\n");
						me->GetChatter()->GuardingHostages(area->GetPlace(), IS_PLAN);
						return;
					}
				}
			}
		}
	}
	else if (TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_ESCAPE)
	{
		if (me->m_iTeam == CT)
		{
			if (me->GuardRandomZone())
			{
				me->SetTask(CCSBot::GUARD_VIP_ESCAPE_ZONE);
				me->PrintIfWatched("Trying to beat them to an escape zone!\n");
				me->SetDisposition(CCSBot::OPPORTUNITY_FIRE);
				me->GetChatter()->GuardingHostageEscapeZone(IS_PLAN);
				return;
			}
		}
		if (me->m_iTeam == TERRORIST)
		{
			me->SetTask(CCSBot::VIP_ESCAPE);
			me->SetDisposition(CCSBot::DispositionType::SELF_DEFENSE);

			//в первую очередь ищем основное оружие
			CArmoury* prim_ent = nullptr;
			bool has_any_primary = me->m_bHasPrimary;
			bool has_primary_on_map = false;

			if (!has_any_primary)
			{
				while (prim_ent = UTIL_FindEntityByClassname(prim_ent, "armoury_entity"))
				{
					if (prim_ent != nullptr)
					{
						if (prim_ent->m_iCount > 0 &&
							prim_ent->m_iItem == ARMOURY_MP5NAVY ||
							prim_ent->m_iItem == ARMOURY_TMP ||
							prim_ent->m_iItem == ARMOURY_P90 ||
							prim_ent->m_iItem == ARMOURY_MAC10 ||
							prim_ent->m_iItem == ARMOURY_AK47 ||
							prim_ent->m_iItem == ARMOURY_SG552 ||
							prim_ent->m_iItem == ARMOURY_M4A1 ||
							prim_ent->m_iItem == ARMOURY_AUG ||
							prim_ent->m_iItem == ARMOURY_SCOUT ||
							prim_ent->m_iItem == ARMOURY_G3SG1 ||
							prim_ent->m_iItem == ARMOURY_AWP ||
							prim_ent->m_iItem == ARMOURY_M3 ||
							prim_ent->m_iItem == ARMOURY_XM1014 ||
							prim_ent->m_iItem == ARMOURY_M249 ||
							prim_ent->m_iItem == ARMOURY_FAMAS ||
							prim_ent->m_iItem == ARMOURY_SG550 ||
							prim_ent->m_iItem == ARMOURY_GALIL ||
							prim_ent->m_iItem == ARMOURY_UMP45
							)
						{
							has_primary_on_map = true;
							break;
						}
					}
				}
			}

			//ищем гранаты
			CArmoury* grenade_ent = nullptr;
			bool has_any_grenade = me->HasGrenade();
			bool has_grenades_on_map = false;

			if (!has_any_grenade)
			{
				while (grenade_ent = UTIL_FindEntityByClassname(grenade_ent, "armoury_entity"))
				{
					if (grenade_ent != nullptr)
					{
						if (grenade_ent->m_iItem == ARMOURY_FLASHBANG || grenade_ent->m_iItem == ARMOURY_HEGRENADE || grenade_ent->m_iItem == ARMOURY_SMOKEGRENADE && grenade_ent->m_iCount > 0)
						{
							has_grenades_on_map = true;
							break;
						}
					}
				}
			}

			//ищем броню
			CArmoury* armor_ent = nullptr;
			bool has_any_armor = me->pev->armorvalue > 0;
			bool has_armor_on_map = false;

			if (!has_any_armor)
			{
				while (armor_ent = UTIL_FindEntityByClassname(armor_ent, "armoury_entity"))
				{
					if (armor_ent != nullptr && (armor_ent->m_iItem == ARMOURY_KEVLAR || armor_ent->m_iItem == ARMOURY_ASSAULT) && armor_ent->m_iCount > 0)
					{
						has_armor_on_map = true;
						break;
					}
				}
			}

			//пиздуем за основным
			if (prim_ent != nullptr && !has_any_primary && has_primary_on_map && prim_ent->m_iCount > 0)
			{
				Vector ent_point = (prim_ent->pev->absmax + prim_ent->pev->absmin) / 2.0f;
				me->MoveTo(&ent_point, RouteType::FASTEST_ROUTE);
				me->PrintIfWatched("I known where primary weapon!\n");
				return;
			}

			//пиздуем за гранатами
			if (grenade_ent != nullptr && !has_any_grenade && has_grenades_on_map && grenade_ent->m_iCount > 0)
			{
				Vector ent_point = (grenade_ent->pev->absmax + grenade_ent->pev->absmin) / 2.0f;
				me->MoveTo(&ent_point, RouteType::FASTEST_ROUTE);
				me->PrintIfWatched("I known where grenades!\n");
				return;
			}

			//пиздуем за броней
			if (armor_ent != nullptr && !has_any_armor && has_armor_on_map && armor_ent->m_iCount > 0)
			{
				Vector ent_point = (armor_ent->pev->absmax + armor_ent->pev->absmin) / 2.0f;
				me->MoveTo(&ent_point, RouteType::FASTEST_ROUTE);
				me->PrintIfWatched("I known where armor!\n");
				return;
			}
		}
	}
	else if (TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_ZOMBIE_MOD)
	{
		if (me->m_iTeam == TERRORIST && cv_bot_zombie_mod_started.value > 0)
		{
			//ходим, хуярим всех
			me->SetTask(CCSBot::SEEK_AND_DESTROY);
			me->SetDisposition(CCSBot::ENGAGE_AND_INVESTIGATE);
		}
		else
		{
			//боимся прячемся
			me->SetTask(CCSBot::FOLLOW);
			me->SetDisposition(CCSBot::OPPORTUNITY_FIRE);

			if (RANDOM_FLOAT(0, 100) < 20)
			{
				if (me->TryToHide(nullptr, RANDOM_FLOAT(3, 90), RANDOM_FLOAT(500, 8192), RANDOM_LONG(0, 1) ? true : false, RANDOM_LONG(0, 1) ? true : false))
				{
					me->ResetStuckMonitor();
				}
			}
			return;
		}
	}

	// listen for enemy noises
	if (me->ShouldInvestigateNoise())
	{
		me->InvestigateNoise();
		return;
	}

	// look around
	me->UpdateLookAround();

	// if we have reached our destination area, pick a new one
	// if our path fails, pick a new one
	if (me->GetLastKnownArea() == m_huntArea || me->UpdatePathMovement() != CCSBot::PROGRESSING)
	{
		m_huntArea = nullptr;
		float oldest = 0.0f;

		int areaCount = 0;
		const float minSize = 150.0f;
		for (auto area : TheNavAreaList)
		{
			areaCount++;

			// skip the small areas
			const Extent *extent = area->GetExtent();
			if (extent->hi.x - extent->lo.x < minSize || extent->hi.y - extent->lo.y < minSize)
				continue;

			// keep track of the least recently cleared area
			real_t age = gpGlobals->time - area->GetClearedTimestamp(me->m_iTeam - 1);
			if (age > oldest)
			{
				oldest = age;
				m_huntArea = area;
			}
		}

		// if all the areas were too small, pick one at random
		int which = RANDOM_LONG(0, areaCount - 1);

		areaCount = 0;
		for (auto area : TheNavAreaList)
		{
			m_huntArea = area;

			if (which == areaCount)
				break;

			which--;
		}

		if (m_huntArea)
		{
			// create a new path to a far away area of the map
			me->ComputePath(m_huntArea, nullptr, SAFEST_ROUTE);
		}
	}
}

// Done hunting
void HuntState::OnExit(CCSBot *me)
{
	;
}
