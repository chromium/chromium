// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_WAKE_UP_BUDGET_POOL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_WAKE_UP_BUDGET_POOL_H_

#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"

#include "base/macros.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace blink {
namespace scheduler {

// WakeUpBudgetPool represents a collection of task queues which run for a
// limited time at regular intervals.
class PLATFORM_EXPORT WakeUpBudgetPool : public BudgetPool {
 public:
  WakeUpBudgetPool(const char* name,
                   BudgetPoolController* budget_pool_controller,
                   base::TimeTicks now);
  ~WakeUpBudgetPool() override;

  // Sets the interval between wake ups. This can be invoked at any time. If a
  // next wake up is already scheduled, it is rescheduled according to the new
  // |interval| as part of this call.
  void SetWakeUpInterval(base::TimeTicks now, base::TimeDelta interval);

  // Sets the duration of wake ups. This does not have an immediate effect and
  // should be called only during initialization of a WakeUpBudgetPool.
  void SetWakeUpDuration(base::TimeDelta duration);

  // If called, the budget pool allows an unaligned wake up when there hasn't
  // been a wake up in the last |wake_up_interval_|.
  //
  // This does not have an immediate effect and should be called only during
  // initialization of a WakeUpBudgetPool.
  void AllowUnalignedWakeUpIfNoRecentWakeUp();

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
  void WriteIntoTrace(perfetto::TracedValue context,
                      base::TimeTicks now) const final;

  absl::optional<base::TimeTicks> last_wake_up_for_testing() const {
    return last_wake_up_;
  }

 protected:
  QueueBlockType GetBlockType() const final;

 private:
  base::TimeDelta wake_up_interval_;
  base::TimeDelta wake_up_duration_;

  bool allow_unaligned_wake_up_is_no_recent_wake_up_ = false;

  absl::optional<base::TimeTicks> last_wake_up_;

  DISALLOW_COPY_AND_ASSIGN(WakeUpBudgetPool);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_WAKE_UP_BUDGET_POOL_H_
