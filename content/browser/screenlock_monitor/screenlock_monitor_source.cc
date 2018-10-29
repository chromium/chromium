// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screenlock_monitor/screenlock_monitor_source.h"

#include "content/browser/screenlock_monitor/screenlock_monitor.h"

namespace content {

ScreenlockMonitorSource::ScreenlockMonitorSource() = default;
ScreenlockMonitorSource::~ScreenlockMonitorSource() = default;

// static
void ScreenlockMonitorSource::ProcessScreenlockEvent(ScreenlockEvent event_id) {
  ScreenlockMonitor* monitor = ScreenlockMonitor::Get();
  if (!monitor) {
    return;
  }

  switch (event_id) {
    case SCREEN_LOCK_EVENT:
      monitor->NotifyScreenLocked();
      break;
    case SCREEN_UNLOCK_EVENT:
      monitor->NotifyScreenUnlocked();
      break;
  }
}

}  // namespace content
