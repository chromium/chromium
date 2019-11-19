// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_BUDGET_POOL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_BUDGET_POOL_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/task/sequence_manager/lazy_now.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace base {
namespace sequence_manager {
class TaskQueue;
}
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace blink {
namespace scheduler {

class BudgetPoolController;
enum class QueueBlockType;

// BudgetPool represents a group of task queues which share a limit
// on a resource. This limit applies when task queues are already throttled
// by TaskQueueThrottler.
class PLATFORM_EXPORT BudgetPool {
  USING_FAST_MALLOC(BudgetPool);

 public:
  virtual ~BudgetPool();

  const char* Name() const;

  // Report task run time to the budget pool.
  virtual void RecordTaskRunTime(base::sequence_manager::TaskQueue* queue,
                                 base::TimeTicks start_time,
                                 base::TimeTicks end_time) = 0;

  // Returns the earliest time when the next pump can be scheduled to run
  // new tasks.
  virtual base::TimeTicks GetNextAllowedRunTime(
      base::TimeTicks desired_run_time) const = 0;

  // Returns true if a task can run at the given time.
  virtual bool CanRunTasksAt(base::TimeTicks moment, bool is_wake_up) const = 0;

  // Returns a point in time until which tasks are allowed to run.
  // base::nullopt means that there are no known limits.
  virtual base::Optional<base::TimeTicks> GetTimeTasksCanRunUntil(
      base::TimeTicks now,
      bool is_wake_up) const = 0;

  // Notifies budget pool that queue has work with desired run time.
  virtual void OnQueueNextWakeUpChanged(
      base::sequence_manager::TaskQueue* queue,
      base::TimeTicks now,
      base::TimeTicks desired_run_time) = 0;

  // Notifies budget pool that wakeup has happened.
  virtual void OnWakeUp(base::TimeTicks now) = 0;

  // Specify how this budget pool should block affected queues.
  virtual QueueBlockType GetBlockType() const = 0;

  // Returns state for tracing.
  virtual void AsValueInto(base::trace_event::TracedValue* state,
                           base::TimeTicks now) const = 0;

  // Adds |queue| to given pool. If the pool restriction does not allow
  // a task to be run immediately and |queue| is throttled, |queue| becomes
  // disabled.
  void AddQueue(base::TimeTicks now, base::sequence_manager::TaskQueue* queue);

  // Removes |queue| from given pool. If it is throttled, it does not
  // become enabled immediately, but a wake-up is scheduled if needed.
  void RemoveQueue(base::TimeTicks now,
                   base::sequence_manager::TaskQueue* queue);

  // Unlike RemoveQueue, does not schedule a new wake-up for the queue.
  void UnregisterQueue(base::sequence_manager::TaskQueue* queue);

  // Enables this time budget pool. Queues from this pool will be
  // throttled based on their run time.
  void EnableThrottling(base::sequence_manager::LazyNow* now);

  // Disables with time budget pool. Queues from this pool will not be
  // throttled based on their run time. A call to |PumpThrottledTasks|
  // will be scheduled to enable this queues back again and respect
  // timer alignment. Internal budget level will not regenerate with time.
  void DisableThrottling(base::sequence_manager::LazyNow* now);

  bool IsThrottlingEnabled() const;

  // All queues should be removed before calling Close().
  void Close();

  // Block all associated queues and schedule them to run when appropriate.
  void BlockThrottledQueues(base::TimeTicks now);

 protected:
  BudgetPool(const char* name, BudgetPoolController* budget_pool_controller);

  const char* name_;  // NOT OWNED

  BudgetPoolController* budget_pool_controller_;

  HashSet<base::sequence_manager::TaskQueue*> associated_task_queues_;
  bool is_enabled_;

 private:
  void DissociateQueue(base::sequence_manager::TaskQueue* queue);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_BUDGET_POOL_H_
