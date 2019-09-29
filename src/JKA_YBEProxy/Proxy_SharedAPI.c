#include "Proxy_Header.h"

// ==================================================
// IMPORT TABLE
// ==================================================

void Proxy_SharedAPI_LocateGameData(sharedEntity_t* gEnts, int numGEntities, int sizeofGEntity_t, playerState_t* clients, int sizeofGameClient)
{
	proxy.locatedGameData.g_entities = gEnts;
	proxy.locatedGameData.num_entities = numGEntities;
	proxy.locatedGameData.g_entitySize = sizeofGEntity_t;

	proxy.locatedGameData.g_clients = clients;
	proxy.locatedGameData.g_clientSize = sizeofGameClient;
}

void Proxy_SharedAPI_GetUsercmd(int clientNum, usercmd_t* cmd)
{
	cmd->forcesel = 0xFFu;
	cmd->angles[ROLL] = 0;
	
	/*
	proxy.clientData[clientNum].pingIndex++;
	proxy.clientData[clientNum].pingSample[proxy.clientData[clientNum].pingIndex & (PING_SAMPLE - 1)] = cmd->serverTime - proxy.clientData[clientNum].lastCmdServerTime;
	proxy.clientData[clientNum].lastCmdServerTime = cmd->serverTime;
	*/
}

// ==================================================
// EXPORT TABLE
// ==================================================

void Proxy_SharedAPI_ClientConnect(int clientNum, qboolean firstTime, qboolean isBot)
{
	// Doesn't work on the new API
	if (firstTime && !isBot)
	{
		proxy.trap->SendServerCommand(clientNum, va("print \"^5%s (^7%s^5) %s^7\n\"", YBEPROXY_NAME, YBEPROXY_VERSION, YBEPROXY_BY_AUTHOR));
	}
}

void Proxy_SharedAPI_ClientBegin(int clientNum, qboolean allowTeamReset)
{
	if (clientNum >= 0 && clientNum < MAX_CLIENTS)
	{
		proxy.clientData[clientNum].isConnected = qtrue;
	}
}

qboolean Proxy_SharedAPI_ClientCommand(int clientNum)
{
	if (!proxy.clientData[clientNum].isConnected)
	{
		return qfalse;
	}

	char cmd[MAX_TOKEN_CHARS] = { 0 };
	qboolean sayCmd = qfalse;

	proxy.trap->Argv(0, cmd, sizeof(cmd));

	if (!Q_stricmpn(&cmd[0], "jkaDST_", 7))
	{
		proxy.trap->SendServerCommand(-1, va("chat \"^3(Anti-Cheat system) ^7%s^3 got kicked cause of cheating^7\"", proxy.clientData->cleanName));
		proxy.trap->DropClient(clientNum, "(Anti-Cheat system) you got kicked cause of cheating");

		return qfalse;
	}

	// Todo (not sure): Check in the entier command + args?
	char* argsConcat = ConcatArgs(1);

	if (!Q_stricmpn(&cmd[0], "say", 3) || !Q_stricmpn(&cmd[0], "say_team", 8) || !Q_stricmpn(&cmd[0], "tell", 4))
	{
		sayCmd = qtrue;

		// 256 because we don't need more, the chat can handle 150 max char
		// and allowing 256 prevent a message to not be sent instead of being truncated
		// if this is a bit more than 150
		if (strlen(argsConcat) > 256)
		{
			return qfalse;
		}
	}

	char cmd_arg1[MAX_TOKEN_CHARS] = { 0 };
	char cmd_arg2[MAX_TOKEN_CHARS] = { 0 };

	proxy.trap->Argv(1, cmd_arg1, sizeof(cmd_arg1));
	proxy.trap->Argv(2, cmd_arg2, sizeof(cmd_arg2));

	// Fix: crach gc
	if (!Q_stricmpn(&cmd[0], "gc", 2) && atoi(cmd_arg1) >= proxy.trap->Cvar_VariableIntegerValue("sv_maxclients"))
	{
		return qfalse;
	}

	// Fix: crash npc spawn
	if (!Q_stricmpn(&cmd[0], "npc", 3) && !Q_stricmpn(&cmd_arg1[0], "spawn", 5) && (!Q_stricmpn(&cmd_arg2[0], "ragnos", 6) || !Q_stricmpn(&cmd_arg2[0], "saber_droid", 6)))
	{
		return qfalse;
	}

	// Fix: team crash
	if (!Q_stricmpn(&cmd[0], "team", 4) && (!Q_stricmpn(&cmd_arg1[0], "follow1", 7) || !Q_stricmpn(&cmd_arg1[0], "follow2", 7)))
	{
		return qfalse;
	}

	// Disable: callteamvote, useless in basejka and can lead to a bugged UI on custom client
	if (!Q_stricmpn(&cmd[0], "callteamvote", 12))
	{
		return qfalse;
	}

	// Fix: callvote fraglimit/timelimit with negative value
	if (!Q_stricmpn(&cmd[0], "callvote", 8) && (!Q_stricmpn(&cmd_arg1[0], "fraglimit", 9) || !Q_stricmpn(&cmd_arg1[0], "timelimit", 9)) && atoi(cmd_arg2) < 0)
	{
		return qfalse;
	}

	if (Q_strchrs(argsConcat, "\r\n"))
	{
		return qfalse;
	}

	if (!sayCmd && Q_strchrs(argsConcat, ";"))
	{
		return qfalse;
	}

	return qtrue;
}

void Proxy_SharedAPI_ClientThink(int clientNum)
{
	
	if (clientNum < 0 || clientNum >= MAX_CLIENTS || !proxy.clientData->isConnected)
	{
		return;
	}

	usercmd_t userCmd;

	proxy.trap->GetUsercmd(clientNum, &userCmd);

	int i;
	int	pingSum = 0;

	//proxy.trap->Print("PING : %d\n", proxy.generalData.previousSvsTime - (proxy.trap->Milliseconds() - proxy.generalData.frameStartTimeMilliseconds) - userCmd.serverTime);

	proxy.clientData->pingSample[proxy.clientData[clientNum].pingIndex & (PING_SAMPLE - 1)] = proxy.generalData.previousSvsTime - (proxy.trap->Milliseconds() - proxy.generalData.frameStartTimeMilliseconds) - userCmd.serverTime;
	proxy.clientData[clientNum].pingIndex++;

	for (i = 0; i < PING_SAMPLE; i++)
	{
		pingSum += proxy.clientData->pingSample[i];
	}

	playerState_t *ps = Proxy_GetPlayerStateByClientNum(clientNum);
	ps->ping = (pingSum && ((pingSum + 50) / PING_SAMPLE) < 0) ? 0 : (pingSum / PING_SAMPLE);
}

void Proxy_SharedAPI_ClientUserinfoChanged(int clientNum)
{
	char userinfo[MAX_INFO_STRING];
	
	proxy.trap->GetUserinfo(clientNum, userinfo, sizeof(userinfo));

	if (strlen(userinfo) <= 0)
	{
		return;
	}

	int len = 0;
	char* val = NULL;

	val = Info_ValueForKey(userinfo, "name");

	// Fix bad names
	Proxy_ClientCleanName(val, proxy.clientData->cleanName, sizeof(proxy.clientData->cleanName));
	Info_SetValueForKey(userinfo, "name", proxy.clientData->cleanName);

	val = Info_ValueForKey(userinfo, "model");

	// Fix bugged models
	if (val)
	{
		len = (int)strlen(val);

		if (!Q_stricmpn(val, "darksidetools", len))
		{
			proxy.trap->SendServerCommand(-1, va("chat \"^3(Anti-Cheat system) ^7%s^3 got kicked cause of cheating^7\"", proxy.clientData->cleanName));
			proxy.trap->DropClient(clientNum, "(Anti-Cheat system) you got kicked cause of cheating");
		}
		
		qboolean badModel = qfalse;

		if (!Q_stricmpn(val, "jedi_", 5) && (!Q_stricmpn(val, "jedi_/red", len) || !Q_stricmpn(val, "jedi_/blue", len)))
			badModel = qtrue;
		else if (!Q_stricmpn(val, "rancor", len))
			badModel = qtrue;
		else if (!Q_stricmpn(val, "wampa", len))
			badModel = qtrue;

		if (badModel)
			Info_SetValueForKey(userinfo, "model", "kyle");
	}

	//Fix forcepowers crash
	char forcePowers[30];
	
	Q_strncpyz(forcePowers, Info_ValueForKey(userinfo, "forcepowers"), sizeof(forcePowers));

	len = (int)strlen(forcePowers);

	qboolean badForce = qfalse;

	if (len >= 22 && len <= 24)
	{
		byte seps = 0;

		for (int i = 0; i < len; i++)
		{
			if (forcePowers[i] != '-' && (forcePowers[i] < '0' || forcePowers[i] > '9'))
			{
				badForce = qtrue;
				break;
			}

			if (forcePowers[i] == '-' && (i < 1 || i > 5))
			{
				badForce = qtrue;
				break;
			}

			if (i && forcePowers[i - 1] == '-' && forcePowers[i] == '-')
			{
				badForce = qtrue;
				break;
			}

			if (forcePowers[i] == '-')
			{
				seps++;
			}
		}

		if (seps != 2)
		{
			badForce = qtrue;
		}
	}
	else
	{
		badForce = qtrue;
	}

	if (badForce)
		Q_strncpyz(forcePowers, "7-1-030000000000003332", sizeof(forcePowers));

	Info_SetValueForKey(userinfo, "forcepowers", forcePowers);

	proxy.trap->SetUserinfo(clientNum, userinfo);

	return;
}