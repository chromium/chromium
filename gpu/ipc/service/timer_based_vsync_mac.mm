// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/timer_based_vsync_mac.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/power_monitor/power_monitor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"

namespace gpu {

// static
TimerBasedVsyncMac* TimerBasedVsyncMac::GetInstance() {
  return base::Singleton<TimerBasedVsyncMac>::get();
}

TimerBasedVsyncMac::TimerBasedVsyncMac() {
  // Use delay based timer for GpuVSync when it fails to create DisplayLinkMac.
  tick_closure_ = base::BindRepeating(&TimerBasedVsyncMac::OnTimerTick,
                                      base::Unretained(this));

  timer_.SetTaskRunner(base::SingleThreadTaskRunner::GetCurrentDefault());

  DCHECK(base::PowerMonitor::IsInitialized());
  is_suspended_ =
      base::PowerMonitor::AddPowerSuspendObserverAndReturnSuspendedState(this);
}

TimerBasedVsyncMac::~TimerBasedVsyncMac() {
  if (timer_.IsRunning()) {
    timer_.Stop();
  }
  timer_callbacks_.clear();

  base::PowerMonitor::RemovePowerSuspendObserver(this);
}

void TimerBasedVsyncMac::AddVSyncTimerCallback(viz::GpuVSyncCallback callback) {
  timer_callbacks_.push_back(callback);

  if (!is_suspended_ && !timer_.IsRunning()) {
    last_target_ = base::TimeTicks::Now() + nominal_refresh_period_;
    timer_.Start(FROM_HERE, last_target_, tick_closure_,
                 base::subtle::DelayPolicy::kPrecise);
  }
}

void TimerBasedVsyncMac::RemoveVSyncTimerCallback(
    viz::GpuVSyncCallback callback) {
  auto timer_it = base::ranges::find(timer_callbacks_, callback);
  if (timer_it != timer_callbacks_.end()) {
    timer_callbacks_.erase(timer_it);
  }

  if (timer_callbacks_.empty() && timer_.IsRunning()) {
    timer_.Stop();
  }
}

// Timer will not stop automatically in power suspension.
void TimerBasedVsyncMac::OnSuspend() {
  if (is_suspended_) {
    return;
  }
  is_suspended_ = true;

  if (timer_.IsRunning()) {
    DCHECK(!timer_callbacks_.empty());
    timer_.Stop();
  }
}

void TimerBasedVsyncMac::OnResume() {
  if (!is_suspended_) {
    return;
  }
  is_suspended_ = false;

  if (!timer_callbacks_.empty()) {
    last_target_ = base::TimeTicks::Now() + nominal_refresh_period_;
    timer_.Start(FROM_HERE, last_target_, tick_closure_,
                 base::subtle::DelayPolicy::kPrecise);
  }
}

void TimerBasedVsyncMac::OnTimerTick() {
  base::TimeTicks now = base::TimeTicks::Now();
  for (auto& callback : timer_callbacks_) {
    callback.Run(now, nominal_refresh_period_);
  }

  base::TimeTicks next_target = now + nominal_refresh_period_;

  timer_.Start(FROM_HERE, next_target, tick_closure_,
               base::subtle::DelayPolicy::kPrecise);
  last_target_ = next_target;
}

}  // namespace gpu
