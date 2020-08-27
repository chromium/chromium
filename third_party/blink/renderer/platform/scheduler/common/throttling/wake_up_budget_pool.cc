// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/throttling/wake_up_budget_pool.h"

#include <algorithm>
#include <cstdint>

#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;

WakeUpBudgetPool::WakeUpBudgetPool(const char* name,
                                   BudgetPoolController* budget_pool_controller,
                                   base::TimeTicks now)
    : BudgetPool(name, budget_pool_controller),
      wake_up_interval_(base::TimeDelta::FromSeconds(1)) {}

WakeUpBudgetPool::~WakeUpBudgetPool() = default;

QueueBlockType WakeUpBudgetPool::GetBlockType() const {
  return QueueBlockType::kNewTasksOnly;
}

void WakeUpBudgetPool::SetWakeUpInterval(base::TimeTicks now,
                                         base::TimeDelta interval) {
  wake_up_interval_ = interval;
  UpdateThrottlingStateForAllQueues(now);
}

void WakeUpBudgetPool::SetWakeUpDuration(base::TimeDelta duration) {
  wake_up_duration_ = duration;
}

void WakeUpBudgetPool::AllowUnalignedWakeUpIfNoRecentWakeUp() {
  allow_unaligned_wake_up_is_no_recent_wake_up_ = true;
}

void WakeUpBudgetPool::RecordTaskRunTime(TaskQueue* queue,
                                         base::TimeTicks start_time,
                                         base::TimeTicks end_time) {
  budget_pool_controller_->UpdateQueueSchedulingLifecycleState(end_time, queue);
}

bool WakeUpBudgetPool::CanRunTasksAt(base::TimeTicks moment,
                                     bool is_wake_up) const {
  if (!is_enabled_)
    return true;
  if (!last_wake_up_)
    return false;
  // |is_wake_up| flag means that we're in the beginning of the wake-up and
  // |OnWakeUp| has just been called. This is needed to support
  // backwards compatibility with old throttling mechanism (when
  // |wake_up_duration| is zero) and allow only one task to run.
  if (last_wake_up_ == moment && is_wake_up)
    return true;
  return moment < last_wake_up_.value() + wake_up_duration_;
}

base::TimeTicks WakeUpBudgetPool::GetTimeTasksCanRunUntil(
    base::TimeTicks now,
    bool is_wake_up) const {
  if (!is_enabled_)
    return base::TimeTicks::Max();
  if (!last_wake_up_)
    return base::TimeTicks();
  if (!CanRunTasksAt(now, is_wake_up))
    return base::TimeTicks();
  return last_wake_up_.value() + wake_up_duration_;
}

base::TimeTicks WakeUpBudgetPool::GetNextAllowedRunTime(
    base::TimeTicks desired_run_time) const {
  if (!is_enabled_)
    return desired_run_time;

  // Do not throttle if the desired run time is still within the duration of the
  // last wake up.
  if (last_wake_up_.has_value() &&
      desired_run_time < last_wake_up_.value() + wake_up_duration_) {
    return desired_run_time;
  }

  // Do not throttle if there hasn't been a wake up in the last wake up
  // interval.
  if (allow_unaligned_wake_up_is_no_recent_wake_up_) {
    // If unaligned wake ups are allowed, the first wake up can happen at any
    // point.
    if (!last_wake_up_.has_value())
      return desired_run_time;

    // Unaligned wake ups can happen at most every |wake_up_interval_| after the
    // last wake up.
    auto next_unaligned_wake_up =
        std::max(desired_run_time, last_wake_up_.value() + wake_up_interval_);

    // Aligned wake ups happen every |wake_up_interval_|, snapped to the minute.
    auto next_aligned_wake_up = desired_run_time.SnappedToNextTick(
        base::TimeTicks(), wake_up_interval_);

    // Pick the earliest of the two allowed run times.
    return std::min(next_unaligned_wake_up, next_aligned_wake_up);
  }

  return desired_run_time.SnappedToNextTick(base::TimeTicks(),
                                            wake_up_interval_);
}

void WakeUpBudgetPool::OnQueueNextWakeUpChanged(
    TaskQueue* queue,
    base::TimeTicks now,
    base::TimeTicks desired_run_time) {
  budget_pool_controller_->UpdateQueueSchedulingLifecycleState(now, queue);
}

void WakeUpBudgetPool::OnWakeUp(base::TimeTicks now) {
  last_wake_up_ = now;
}

void WakeUpBudgetPool::AsValueInto(base::trace_event::TracedValue* state,
                                   base::TimeTicks now) const {
  auto dictionary_scope = state->BeginDictionaryScoped(name_);

  state->SetString("name", name_);
  state->SetDouble("wake_up_interval_in_seconds",
                   wake_up_interval_.InSecondsF());
  state->SetDouble("wake_up_duration_in_seconds",
                   wake_up_duration_.InSecondsF());
  if (last_wake_up_) {
    state->SetDouble("last_wake_up_seconds_ago",
                     (now - last_wake_up_.value()).InSecondsF());
  }
  state->SetBoolean("is_enabled", is_enabled_);

  {
    auto array_scope = state->BeginArrayScoped("task_queues");
    for (TaskQueue* queue : associated_task_queues_) {
      state->AppendString(PointerToString(queue));
    }
  }
}

}  // namespace scheduler
}  // namespace blink
