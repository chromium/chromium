// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_CPU_TIME_BUDGET_POOL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_CPU_TIME_BUDGET_POOL_H_

#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"

namespace blink {
namespace scheduler {

// CPUTimeBudgetPool represents a collection of task queues which share a limit
// on total cpu time.
class PLATFORM_EXPORT CPUTimeBudgetPool : public BudgetPool {
 public:
  CPUTimeBudgetPool(const char* name,
                    BudgetPoolController* budget_pool_controller,
                    TraceableVariableController* tracing_controller,
                    base::TimeTicks now);

  ~CPUTimeBudgetPool() override;

  // Set max budget level, base::nullopt represent absence of max level.
  // Max budget level prevents accumulating arbitrary large budgets when
  // page is inactive for a very long time.
  void SetMaxBudgetLevel(base::TimeTicks now,
                         base::Optional<base::TimeDelta> max_budget_level);

  // Set max throttling duration, base::nullopt represents absense of it.
  // Max throttling duration prevents page from being throttled for
  // a very long period after a single long task.
  void SetMaxThrottlingDelay(
      base::TimeTicks now,
      base::Optional<base::TimeDelta> max_throttling_delay);

  // Set minimal budget level required to run a task. If budget pool was
  // exhausted, it needs to accumulate at least |min_budget_to_run| time units
  // to unblock and run tasks again. When unblocked, it still can run tasks
  // when budget is positive but less than this level until being blocked
  // until being blocked when budget reaches zero.
  // This is needed for integration with WakeUpBudgetPool to prevent a situation
  // when wake-up happened but time budget pool allows only one task to run at
  // the moment.
  // It is recommended to use the same value for this and WakeUpBudgetPool's
  // wake-up window length.
  // NOTE: This does not have an immediate effect and does not call
  // BudgetPoolController::UnblockQueue.
  void SetMinBudgetLevelToRun(base::TimeTicks now,
                              base::TimeDelta min_budget_level_to_run);

  // Throttle task queues from this time budget pool if tasks are running
  // for more than |cpu_percentage| per cent of wall time.
  // This function does not affect internal time budget level.
  void SetTimeBudgetRecoveryRate(base::TimeTicks now, double cpu_percentage);

  // Increase budget level by given value. This function DOES NOT unblock
  // queues even if they are allowed to run with increased budget level.
  void GrantAdditionalBudget(base::TimeTicks now, base::TimeDelta budget_level);

  // Set callback which will be called every time when this budget pool
  // is throttled. Throttling duration (time until the queue is allowed
  // to run again) is passed as a parameter to callback.
  void SetReportingCallback(
      base::RepeatingCallback<void(base::TimeDelta)> reporting_callback);

  // BudgetPool implementation:
  void RecordTaskRunTime(base::sequence_manager::TaskQueue* queue,
                         base::TimeTicks start_time,
                         base::TimeTicks end_time) final;
  bool CanRunTasksAt(base::TimeTicks moment, bool is_wake_up) const final;
  base::TimeTicks GetTimeTasksCanRunUntil(base::TimeTicks now,
                                          bool is_wake_up) const final;
  base::TimeTicks GetNextAllowedRunTime(
      base::TimeTicks desired_run_time) const final;
  void OnQueueNextWakeUpChanged(base::sequence_manager::TaskQueue* queue,
                                base::TimeTicks now,
                                base::TimeTicks desired_run_time) final;
  void OnWakeUp(base::TimeTicks now) final;
  void WriteIntoTracedValue(perfetto::TracedValue context,
                            base::TimeTicks) const final;

 protected:
  QueueBlockType GetBlockType() const final;

 private:
  FRIEND_TEST_ALL_PREFIXES(TaskQueueThrottlerTest, CPUTimeBudgetPool);

  // Advances |last_checkpoint_| to |now| if needed and recalculates
  // budget level.
  void Advance(base::TimeTicks now);

  // Increase |current_budget_level_| to satisfy max throttling duration
  // condition if necessary.
  // Decrease |current_budget_level_| to satisfy max budget level
  // condition if necessary.
  void EnforceBudgetLevelRestrictions();

  // Max budget level which we can accrue.
  // Tasks will be allowed to run for this time before being throttled
  // after a very long period of inactivity.
  base::Optional<base::TimeDelta> max_budget_level_;
  // Max throttling delay places a lower limit on time budget level,
  // ensuring that one long task does not cause extremely long throttling.
  // Note that this is not a guarantee that every task will run
  // after desired run time + max throttling duration, but a guarantee
  // that at least one task will be run every max_throttling_delay.
  base::Optional<base::TimeDelta> max_throttling_delay_;
  // See CPUTimeBudgetPool::SetMinBudgetLevelToRun.
  base::TimeDelta min_budget_level_to_run_;

  TraceableCounter<base::TimeDelta, TracingCategoryName::kInfo>
      current_budget_level_;
  base::TimeTicks last_checkpoint_;
  double cpu_percentage_;

  base::RepeatingCallback<void(base::TimeDelta)> reporting_callback_;

  DISALLOW_COPY_AND_ASSIGN(CPUTimeBudgetPool);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_CPU_TIME_BUDGET_POOL_H_
