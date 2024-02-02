// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/throttling/cpu_time_budget_pool.h"

#include <cstdint>
#include <optional>

#include "base/check_op.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;

CPUTimeBudgetPool::CPUTimeBudgetPool(
    const char* name,
    TraceableVariableController* tracing_controller,
    base::TimeTicks now)
    : BudgetPool(name),
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
    std::optional<base::TimeDelta> max_budget_level) {
  Advance(now);
  max_budget_level_ = max_budget_level;
  EnforceBudgetLevelRestrictions();
}

void CPUTimeBudgetPool::SetMaxThrottlingDelay(
    base::TimeTicks now,
    std::optional<base::TimeDelta> max_throttling_delay) {
  Advance(now);
  max_throttling_delay_ = max_throttling_delay;
  EnforceBudgetLevelRestrictions();
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

bool CPUTimeBudgetPool::CanRunTasksAt(base::TimeTicks moment) const {
  if (!is_enabled_)
    return true;
  if (current_budget_level_->InMicroseconds() >= 0)
    return true;
  base::TimeDelta time_to_recover_budget =
      -current_budget_level_ / cpu_percentage_;
  if (moment - last_checkpoint_ >= time_to_recover_budget) {
    return true;
  }

  return false;
}

base::TimeTicks CPUTimeBudgetPool::GetTimeTasksCanRunUntil(
    base::TimeTicks now) const {
  if (CanRunTasksAt(now))
    return base::TimeTicks::Max();
  return base::TimeTicks();
}

base::TimeTicks CPUTimeBudgetPool::GetNextAllowedRunTime(
    base::TimeTicks desired_run_time) const {
  if (!is_enabled_ || current_budget_level_->InMicroseconds() >= 0) {
    return last_checkpoint_;
  }
  // Subtract because current_budget is negative.
  return std::max(desired_run_time, last_checkpoint_ + (-current_budget_level_ /
                                                        cpu_percentage_));
}

void CPUTimeBudgetPool::RecordTaskRunTime(base::TimeTicks start_time,
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
    UpdateStateForAllThrottlers(end_time);
}

void CPUTimeBudgetPool::OnWakeUp(base::TimeTicks now) {}

void CPUTimeBudgetPool::WriteIntoTrace(perfetto::TracedValue context,
                                       base::TimeTicks now) const {
  auto dict = std::move(context).WriteDictionary();

  dict.Add("name", name_);
  dict.Add("time_budget", cpu_percentage_);
  dict.Add("time_budget_level_in_seconds", current_budget_level_->InSecondsF());
  dict.Add("last_checkpoint_seconds_ago",
           (now - last_checkpoint_).InSecondsF());
  dict.Add("is_enabled", is_enabled_);

  if (max_throttling_delay_) {
    dict.Add("max_throttling_delay_in_seconds",
             max_throttling_delay_.value().InSecondsF());
  }
  if (max_budget_level_) {
    dict.Add("max_budget_level_in_seconds",
             max_budget_level_.value().InSecondsF());
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
