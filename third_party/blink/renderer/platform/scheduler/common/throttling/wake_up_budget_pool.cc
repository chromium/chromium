// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/throttling/wake_up_budget_pool.h"

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
      wake_up_interval_(base::TimeDelta::FromSecondsD(1.0)) {}

WakeUpBudgetPool::~WakeUpBudgetPool() = default;

QueueBlockType WakeUpBudgetPool::GetBlockType() const {
  return QueueBlockType::kNewTasksOnly;
}

void WakeUpBudgetPool::SetWakeUpRate(double wake_ups_per_second) {
  wake_up_interval_ = base::TimeDelta::FromSecondsD(1 / wake_ups_per_second);
}

void WakeUpBudgetPool::SetWakeUpDuration(base::TimeDelta duration) {
  wake_up_duration_ = duration;
}

void WakeUpBudgetPool::RecordTaskRunTime(TaskQueue* queue,
                                         base::TimeTicks start_time,
                                         base::TimeTicks end_time) {
  budget_pool_controller_->UpdateQueueSchedulingLifecycleState(end_time, queue);
}

bool WakeUpBudgetPool::CanRunTasksAt(base::TimeTicks moment,
                                     bool is_wake_up) const {
  if (!last_wake_up_)
    return false;
  // |is_wake_up| flag means that we're in the beginning of the wake-up and
  // |OnWakeUp| has just been called. This is needed to support backwards
  // compability with old throttling mechanism (when |wake_up_duration| is zero)
  // and allow only one task to run.
  if (last_wake_up_ == moment && is_wake_up)
    return true;
  return moment < last_wake_up_.value() + wake_up_duration_;
}

base::Optional<base::TimeTicks> WakeUpBudgetPool::GetTimeTasksCanRunUntil(
    base::TimeTicks now,
    bool is_wake_up) const {
  if (!last_wake_up_)
    return base::TimeTicks();
  if (!CanRunTasksAt(now, is_wake_up))
    return base::TimeTicks();
  return last_wake_up_.value() + wake_up_duration_;
}

namespace {

// Wrapper around base::TimeTicks::SnappedToNextTick which ensures that
// the returned point is strictly in the future.
base::TimeTicks SnapToNextTickStrict(base::TimeTicks moment,
                                     base::TimeDelta interval) {
  base::TimeTicks snapped =
      moment.SnappedToNextTick(base::TimeTicks(), interval);
  if (snapped == moment)
    return moment + interval;
  return snapped;
}

}  // namespace

base::TimeTicks WakeUpBudgetPool::GetNextAllowedRunTime(
    base::TimeTicks desired_run_time) const {
  if (!last_wake_up_)
    return SnapToNextTickStrict(desired_run_time, wake_up_interval_);
  if (desired_run_time < last_wake_up_.value() + wake_up_duration_)
    return desired_run_time;
  return SnapToNextTickStrict(std::max(desired_run_time, last_wake_up_.value()),
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
  state->BeginDictionary(name_);

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

  state->BeginArray("task_queues");
  for (TaskQueue* queue : associated_task_queues_) {
    state->AppendString(PointerToString(queue));
  }
  state->EndArray();

  state->EndDictionary();
}

}  // namespace scheduler
}  // namespace blink
