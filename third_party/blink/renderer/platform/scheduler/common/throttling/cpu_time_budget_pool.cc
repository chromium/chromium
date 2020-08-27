// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/throttling/cpu_time_budget_pool.h"

#include <cstdint>

#include "base/check_op.h"
#include "base/optional.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;

CPUTimeBudgetPool::CPUTimeBudgetPool(
    const char* name,
    BudgetPoolController* budget_pool_controller,
    TraceableVariableController* tracing_controller,
    base::TimeTicks now)
    : BudgetPool(name, budget_pool_controller),
      current_budget_level_(base::TimeDelta(),
                            "RendererScheduler.BackgroundBudgetMs",
                            tracing_controller,
                            TimeDeltaToMilliseconds),
      last_checkpoint_(now),
      cpu_percentage_(1) {}

CPUTimeBudgetPool::~CPUTimeBudgetPool() = default;

QueueBlockType CPUTimeBudgetPool::GetBlockType() const {
  return QueueBlockType::kAllTasks;
}

void CPUTimeBudgetPool::SetMaxBudgetLevel(
    base::TimeTicks now,
    base::Optional<base::TimeDelta> max_budget_level) {
  Advance(now);
  max_budget_level_ = max_budget_level;
  EnforceBudgetLevelRestrictions();
}

void CPUTimeBudgetPool::SetMaxThrottlingDelay(
    base::TimeTicks now,
    base::Optional<base::TimeDelta> max_throttling_delay) {
  Advance(now);
  max_throttling_delay_ = max_throttling_delay;
  EnforceBudgetLevelRestrictions();
}

void CPUTimeBudgetPool::SetMinBudgetLevelToRun(
    base::TimeTicks now,
    base::TimeDelta min_budget_level_to_run) {
  Advance(now);
  min_budget_level_to_run_ = min_budget_level_to_run;
}

void CPUTimeBudgetPool::SetTimeBudgetRecoveryRate(base::TimeTicks now,
                                                  double cpu_percentage) {
  Advance(now);
  cpu_percentage_ = cpu_percentage;
  EnforceBudgetLevelRestrictions();
}

void CPUTimeBudgetPool::GrantAdditionalBudget(base::TimeTicks now,
                                              base::TimeDelta budget_level) {
  Advance(now);
  current_budget_level_ += budget_level;
  EnforceBudgetLevelRestrictions();
}

void CPUTimeBudgetPool::SetReportingCallback(
    base::RepeatingCallback<void(base::TimeDelta)> reporting_callback) {
  reporting_callback_ = reporting_callback;
}

bool CPUTimeBudgetPool::CanRunTasksAt(base::TimeTicks moment,
                                      bool is_wake_up) const {
  return moment >= GetNextAllowedRunTime(moment);
}

base::TimeTicks CPUTimeBudgetPool::GetTimeTasksCanRunUntil(
    base::TimeTicks now,
    bool is_wake_up) const {
  if (CanRunTasksAt(now, is_wake_up))
    return base::TimeTicks::Max();
  return base::TimeTicks();
}

base::TimeTicks CPUTimeBudgetPool::GetNextAllowedRunTime(
    base::TimeTicks desired_run_time) const {
  if (!is_enabled_ || current_budget_level_->InMicroseconds() >= 0)
    return last_checkpoint_;
  // Subtract because current_budget is negative.
  return last_checkpoint_ +
         (-current_budget_level_ + min_budget_level_to_run_) / cpu_percentage_;
}

void CPUTimeBudgetPool::RecordTaskRunTime(TaskQueue* queue,
                                          base::TimeTicks start_time,
                                          base::TimeTicks end_time) {
  DCHECK_LE(start_time, end_time);
  Advance(end_time);
  if (is_enabled_) {
    base::TimeDelta old_budget_level = current_budget_level_;
    current_budget_level_ -= (end_time - start_time);
    EnforceBudgetLevelRestrictions();

    if (!reporting_callback_.is_null() && old_budget_level.InSecondsF() > 0 &&
        current_budget_level_->InSecondsF() < 0) {
      reporting_callback_.Run(-current_budget_level_ / cpu_percentage_);
    }
  }

  if (current_budget_level_->InSecondsF() < 0)
    UpdateThrottlingStateForAllQueues(end_time);
}

void CPUTimeBudgetPool::OnQueueNextWakeUpChanged(
    TaskQueue* queue,
    base::TimeTicks now,
    base::TimeTicks desired_run_time) {
  budget_pool_controller_->UpdateQueueSchedulingLifecycleState(now, queue);
}

void CPUTimeBudgetPool::OnWakeUp(base::TimeTicks now) {}

void CPUTimeBudgetPool::AsValueInto(base::trace_event::TracedValue* state,
                                    base::TimeTicks now) const {
  current_budget_level_.Trace();
  auto dictionary_scope = state->BeginDictionaryScoped(name_);

  state->SetString("name", name_);
  state->SetDouble("time_budget", cpu_percentage_);
  state->SetDouble("time_budget_level_in_seconds",
                   current_budget_level_->InSecondsF());
  state->SetDouble("last_checkpoint_seconds_ago",
                   (now - last_checkpoint_).InSecondsF());
  state->SetBoolean("is_enabled", is_enabled_);
  state->SetDouble("min_budget_level_to_run_in_seconds",
                   min_budget_level_to_run_.InSecondsF());

  if (max_throttling_delay_) {
    state->SetDouble("max_throttling_delay_in_seconds",
                     max_throttling_delay_.value().InSecondsF());
  }
  if (max_budget_level_) {
    state->SetDouble("max_budget_level_in_seconds",
                     max_budget_level_.value().InSecondsF());
  }

  {
    auto array_scope = state->BeginArrayScoped("task_queues");
    for (TaskQueue* queue : associated_task_queues_) {
      state->AppendString(PointerToString(queue));
    }
  }
}

void CPUTimeBudgetPool::Advance(base::TimeTicks now) {
  if (now > last_checkpoint_) {
    if (is_enabled_) {
      current_budget_level_ += cpu_percentage_ * (now - last_checkpoint_);
      EnforceBudgetLevelRestrictions();
    }
    last_checkpoint_ = now;
  }
}

void CPUTimeBudgetPool::EnforceBudgetLevelRestrictions() {
  if (max_budget_level_) {
    current_budget_level_ =
        std::min(current_budget_level_.value(), max_budget_level_.value());
  }
  if (max_throttling_delay_) {
    // Current budget level may be negative.
    current_budget_level_ =
        std::max(current_budget_level_.value(),
                 -max_throttling_delay_.value() * cpu_percentage_);
  }
}

}  // namespace scheduler
}  // namespace blink
