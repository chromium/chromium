// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/auto_advancing_virtual_time_domain.h"

#include <atomic>

#include "base/time/time_override.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"

namespace blink {
namespace scheduler {

AutoAdvancingVirtualTimeDomain::AutoAdvancingVirtualTimeDomain(
    base::Time initial_time,
    base::TimeTicks initial_time_ticks,
    SchedulerHelper* helper)
    : task_starvation_count_(0),
      max_task_starvation_count_(0),
      can_advance_virtual_time_(true),
      helper_(helper),
      time_override_(ProcessTimeOverrideCoordinator::CreateOverride(
          initial_time,
          initial_time_ticks,
          base::BindRepeating(
              &AutoAdvancingVirtualTimeDomain::NotifyPolicyChanged,
              base::Unretained(this)))),
      initial_time_ticks_(time_override_->NowTicks()) {
  helper_->AddTaskObserver(this);
}

AutoAdvancingVirtualTimeDomain::~AutoAdvancingVirtualTimeDomain() {
  helper_->RemoveTaskObserver(this);
}

base::TimeTicks AutoAdvancingVirtualTimeDomain::NowTicks() const {
  return time_override_->NowTicks();
}

bool AutoAdvancingVirtualTimeDomain::MaybeFastForwardToWakeUp(
    std::optional<base::sequence_manager::WakeUp> wakeup,
    bool quit_when_idle_requested) {
  if (!can_advance_virtual_time_)
    return false;

  if (!wakeup)
    return false;

  if (MaybeAdvanceVirtualTime(wakeup->time)) {
    task_starvation_count_ = 0;
    return true;
  }

  return false;
}

void AutoAdvancingVirtualTimeDomain::SetCanAdvanceVirtualTime(
    bool can_advance_virtual_time) {
  can_advance_virtual_time_ = can_advance_virtual_time;
  if (can_advance_virtual_time_)
    NotifyPolicyChanged();
}

void AutoAdvancingVirtualTimeDomain::SetMaxVirtualTimeTaskStarvationCount(
    int max_task_starvation_count) {
  max_task_starvation_count_ = max_task_starvation_count;
  if (max_task_starvation_count_ == 0)
    task_starvation_count_ = 0;
}

void AutoAdvancingVirtualTimeDomain::SetVirtualTimeFence(
    base::TimeTicks virtual_time_fence) {
  virtual_time_fence_ = virtual_time_fence;
  if (!requested_next_virtual_time_.is_null())
    MaybeAdvanceVirtualTime(requested_next_virtual_time_);
}

bool AutoAdvancingVirtualTimeDomain::MaybeAdvanceVirtualTime(
    base::TimeTicks new_virtual_time) {
  // If set, don't advance past the end of |virtual_time_fence_|.
  if (!virtual_time_fence_.is_null() &&
      new_virtual_time > virtual_time_fence_) {
    requested_next_virtual_time_ = new_virtual_time;
    new_virtual_time = virtual_time_fence_;
  } else {
    requested_next_virtual_time_ = base::TimeTicks();
  }

  // Currently, a virtual time pauser may try to advance time to
  // a value from the past.
  // TODO(caseq): make sure we don't try "advancing" to past values.
  if (new_virtual_time <= NowTicks()) {
    return false;
  }

  return time_override_->TryAdvancingTime(new_virtual_time) == new_virtual_time;
}

const char* AutoAdvancingVirtualTimeDomain::GetName() const {
  return "AutoAdvancingVirtualTimeDomain";
}

void AutoAdvancingVirtualTimeDomain::WillProcessTask(
    const base::PendingTask& pending_task,
    bool was_blocked_or_low_priority) {}

void AutoAdvancingVirtualTimeDomain::DidProcessTask(
    const base::PendingTask& pending_task) {
  if (max_task_starvation_count_ == 0 ||
      ++task_starvation_count_ < max_task_starvation_count_) {
    return;
  }

  // Delayed tasks are being excessively starved, so allow virtual time to
  // advance.
  auto wake_up = helper_->GetNextWakeUp();
  if (wake_up && MaybeAdvanceVirtualTime(wake_up->time))
    task_starvation_count_ = 0;
}

}  // namespace scheduler
}  // namespace blink
