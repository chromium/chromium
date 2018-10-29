// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screenlock_monitor/screenlock_monitor.h"

#include "base/trace_event/trace_event.h"
#include "content/browser/screenlock_monitor/screenlock_monitor_source.h"

namespace content {

static ScreenlockMonitor* g_screenlock_monitor = nullptr;

ScreenlockMonitor::ScreenlockMonitor(
    std::unique_ptr<ScreenlockMonitorSource> source)
    : observers_(new base::ObserverListThreadSafe<ScreenlockObserver>()),
      source_(std::move(source)) {
  DCHECK(!g_screenlock_monitor);
  g_screenlock_monitor = this;
}

ScreenlockMonitor::~ScreenlockMonitor() {
  DCHECK_EQ(this, g_screenlock_monitor);
  g_screenlock_monitor = nullptr;
}

// static
ScreenlockMonitor* ScreenlockMonitor::Get() {
  return g_screenlock_monitor;
}

void ScreenlockMonitor::AddObserver(ScreenlockObserver* obs) {
  observers_->AddObserver(obs);
}

void ScreenlockMonitor::RemoveObserver(ScreenlockObserver* obs) {
  observers_->RemoveObserver(obs);
}

void ScreenlockMonitor::NotifyScreenLocked() {
  TRACE_EVENT_INSTANT0("screenlock_monitor",
                       "ScreenlockMonitor::NotifyScreenLocked",
                       TRACE_EVENT_SCOPE_GLOBAL);
  DVLOG(1) << "Screen Locked";
  observers_->Notify(FROM_HERE, &ScreenlockObserver::OnScreenLocked);
}

void ScreenlockMonitor::NotifyScreenUnlocked() {
  TRACE_EVENT_INSTANT0("screenlock_monitor",
                       "ScreenlockMonitor::NotifyScreenUnlocked",
                       TRACE_EVENT_SCOPE_GLOBAL);
  DVLOG(1) << "Screen Unlocked";
  observers_->Notify(FROM_HERE, &ScreenlockObserver::OnScreenUnlocked);
}

}  // namespace content
