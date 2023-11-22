// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screenlock_monitor/screenlock_monitor.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
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
  RecordAction(base::UserMetricsAction("ScreenlockMonitor_ScreenLocked"));
  ReportLockUnlockDuration(/*is_locked=*/true);
  observers_->Notify(FROM_HERE, &ScreenlockObserver::OnScreenLocked);
}

void ScreenlockMonitor::NotifyScreenUnlocked() {
  TRACE_EVENT_INSTANT0("screenlock_monitor",
                       "ScreenlockMonitor::NotifyScreenUnlocked",
                       TRACE_EVENT_SCOPE_GLOBAL);
  DVLOG(1) << "Screen Unlocked";
  RecordAction(base::UserMetricsAction("ScreenlockMonitor_ScreenUnlocked"));
  ReportLockUnlockDuration(/*is_locked=*/false);
  observers_->Notify(FROM_HERE, &ScreenlockObserver::OnScreenUnlocked);
}

void ScreenlockMonitor::ReportLockUnlockDuration(bool is_locked) {
  const base::TimeTicks now = base::TimeTicks::Now();

  // If it is the first time called, just record the time and lock state.
  if (last_lock_unlock_time_.is_null()) {
    last_lock_unlock_time_ = now;
    is_locked_ = is_locked;
    return;
  }

  // Skip if duplicated lock state is called.
  if (is_locked_ == is_locked)
    return;

  is_locked_ = is_locked;

  // Record metrics.
  if (is_locked_) {
    base::UmaHistogramLongTimes("ScreenLocker.Unlocked.Duration",
                                now - last_lock_unlock_time_);
  } else {
    base::UmaHistogramLongTimes("ScreenLocker.Locked.Duration",
                                now - last_lock_unlock_time_);
  }
  last_lock_unlock_time_ = now;
}

}  // namespace content
