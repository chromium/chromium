// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screenlock_monitor/screenlock_monitor_device_source.h"

#include "base/trace_event/trace_event.h"
#include "components/session_manager/core/session_manager.h"

namespace content {

ScreenlockMonitorDeviceSource::ScreenLockListener::ScreenLockListener() {
  if (session_manager::SessionManager::Get()) {
    session_manager::SessionManager::Get()->AddObserver(this);
  }
}

ScreenlockMonitorDeviceSource::ScreenLockListener::~ScreenLockListener() {
  if (session_manager::SessionManager::Get()) {
    session_manager::SessionManager::Get()->RemoveObserver(this);
  }
}

void ScreenlockMonitorDeviceSource::ScreenLockListener::
    OnSessionStateChanged() {
  TRACE_EVENT0("login,screenlock_monitor",
               "ScreenlockMonitorDeviceSource::OnSessionStateChanged");
  ScreenlockEvent screenlock_event;
  if (session_manager::SessionManager::Get()->IsScreenLocked()) {
    screenlock_event = SCREEN_LOCK_EVENT;
  } else {
    screenlock_event = SCREEN_UNLOCK_EVENT;
  }

  if (!prev_event_ || *prev_event_ != screenlock_event) {
    prev_event_ = screenlock_event;
    ProcessScreenlockEvent(screenlock_event);
  }
}

}  // namespace content
