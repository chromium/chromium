// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_TASK_QUEUE_THROTTLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_TASK_QUEUE_THROTTLER_H_

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/time_domain.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/common/cancelable_closure_holder.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool_controller.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/cpu_time_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/wake_up_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace blink {
namespace scheduler {

class BudgetPool;
class ThreadSchedulerImpl;
class ThrottledTimeDomain;
class CPUTimeBudgetPool;
class WakeUpBudgetPool;

// kNewTasksOnly prevents new tasks from running (old tasks can run normally),
// kAllTasks block queue completely.
// kAllTasks-type block always blocks the queue completely.
// kNewTasksOnly-type block does nothing when queue is already blocked by
// kAllTasks, and overrides previous kNewTasksOnly block if any, which may
// unblock some tasks.
enum class QueueBlockType { kAllTasks, kNewTasksOnly };

// The job of the TaskQueueThrottler is to control when tasks posted on
// throttled queues get run. The TaskQueueThrottler:
// - runs throttled tasks once per second,
// - controls time budget for task queues grouped in CPUTimeBudgetPools.
//
// This is done by disabling throttled queues and running
// a special "heart beat" function |PumpThrottledTasks| which when run
// temporarily enables throttled queues and inserts a fence to ensure tasks
// posted from a throttled task run next time the queue is pumped.
//
// Of course the TaskQueueThrottler isn't the only sub-system that wants to
// enable or disable queues. E.g. ThreadSchedulerImpl also does this for
// policy reasons. To prevent the systems from fighting, clients of
// TaskQueueThrottler must use SetQueueEnabled rather than calling the function
// directly on the queue.
//
// There may be more than one system that wishes to throttle a queue (e.g.
// renderer suspension vs tab level suspension) so the TaskQueueThrottler keeps
// a count of the number of systems that wish a queue to be throttled.
// See IncreaseThrottleRefCount & DecreaseThrottleRefCount.
//
// This class is main-thread only.
class PLATFORM_EXPORT TaskQueueThrottler : public BudgetPoolController {
 public:
  // We use tracing controller from ThreadSchedulerImpl because an instance
  // of this class is always its member, so has the same lifetime.
  TaskQueueThrottler(ThreadSchedulerImpl* thread_scheduler,
                     TraceableVariableController* tracing_controller);

  ~TaskQueueThrottler() override;

  void OnQueueNextWakeUpChanged(base::sequence_manager::TaskQueue* queue,
                                base::TimeTicks wake_up);

  // BudgetPoolController implementation:
  void AddQueueToBudgetPool(base::sequence_manager::TaskQueue* queue,
                            BudgetPool* budget_pool) override;
  void RemoveQueueFromBudgetPool(base::sequence_manager::TaskQueue* queue,
                                 BudgetPool* budget_pool) override;
  void UnregisterBudgetPool(BudgetPool* budget_pool) override;
  void UpdateQueueSchedulingLifecycleState(
      base::TimeTicks now,
      base::sequence_manager::TaskQueue* queue) override;
  bool IsThrottled(base::sequence_manager::TaskQueue* queue) const override;

  // Increments the throttled refcount and causes |task_queue| to be throttled
  // if its not already throttled.
  void IncreaseThrottleRefCount(base::sequence_manager::TaskQueue* task_queue);

  // If the refcouint is non-zero it's decremented.  If the throttled refcount
  // becomes zero then |task_queue| is unthrottled.  If the refcount was already
  // zero this function does nothing.
  void DecreaseThrottleRefCount(base::sequence_manager::TaskQueue* task_queue);

  // Removes |task_queue| from |queue_details| and from appropriate budget pool.
  void ShutdownTaskQueue(base::sequence_manager::TaskQueue* task_queue);

  // Disable throttling for all queues, this setting takes precedence over
  // all other throttling settings. Designed to be used when a global event
  // disabling throttling happens (e.g. audio is playing).
  void DisableThrottling();

  // Enable back global throttling.
  void EnableThrottling();

  const ThrottledTimeDomain* time_domain() const { return time_domain_.get(); }

  // TODO(altimin): Remove it.
  static base::TimeTicks AlignedThrottledRunTime(
      base::TimeTicks unthrottled_runtime);

  // Returned object is owned by |TaskQueueThrottler|.
  CPUTimeBudgetPool* CreateCPUTimeBudgetPool(const char* name);
  WakeUpBudgetPool* CreateWakeUpBudgetPool(const char* name);

  // Accounts for given task for cpu-based throttling needs.
  void OnTaskRunTimeReported(base::sequence_manager::TaskQueue* task_queue,
                             base::TimeTicks start_time,
                             base::TimeTicks end_time);

  void AsValueInto(base::trace_event::TracedValue* state,
                   base::TimeTicks now) const;

 private:
  class Metadata : public base::sequence_manager::TaskQueue::Observer {
   public:
    Metadata(base::sequence_manager::TaskQueue* queue,
             TaskQueueThrottler* throttler);

    ~Metadata() override;

    // Returns true if |throttling_ref_count_| was zero.
    bool IncrementRefCount();

    // Returns true if |throttling_ref_count_| is now zero.
    bool DecrementRefCount();

    // TaskQueue::Observer implementation:
    void OnPostTask(base::Location from_here, base::TimeDelta delay) override;
    void OnQueueNextWakeUpChanged(base::TimeTicks wake_up) override;

    size_t throttling_ref_count() const { return throttling_ref_count_; }

    const HashSet<BudgetPool*>& budget_pools() const { return budget_pools_; }

    HashSet<BudgetPool*>& budget_pools() { return budget_pools_; }

   private:
    base::sequence_manager::TaskQueue* const queue_;
    TaskQueueThrottler* const throttler_;
    size_t throttling_ref_count_ = 0;
    HashSet<BudgetPool*> budget_pools_;
  };

  using TaskQueueMap =
      HashMap<base::sequence_manager::TaskQueue*, std::unique_ptr<Metadata>>;

  void PumpThrottledTasks();

  // Note |unthrottled_runtime| might be in the past. When this happens we
  // compute the delay to the next runtime based on now rather than
  // unthrottled_runtime.
  void MaybeSchedulePumpThrottledTasks(const base::Location& from_here,
                                       base::TimeTicks now,
                                       base::TimeTicks runtime);

  // Return next possible time when queue is allowed to run in accordance
  // with throttling policy.
  base::TimeTicks GetNextAllowedRunTime(
      base::sequence_manager::TaskQueue* queue,
      base::TimeTicks desired_run_time);

  bool CanRunTasksAt(base::sequence_manager::TaskQueue* queue,
                     base::TimeTicks moment,
                     bool is_wake_up);

  base::Optional<base::TimeTicks> GetTimeTasksCanRunUntil(
      base::sequence_manager::TaskQueue* queue,
      base::TimeTicks now,
      bool is_wake_up) const;

  void MaybeDeleteQueueMetadata(TaskQueueMap::iterator it);

  void UpdateQueueSchedulingLifecycleStateInternal(
      base::TimeTicks now,
      base::sequence_manager::TaskQueue* queue,
      bool is_wake_up);

  base::Optional<QueueBlockType> GetQueueBlockType(
      base::TimeTicks now,
      base::sequence_manager::TaskQueue* queue);

  TaskQueueMap queue_details_;
  base::RepeatingCallback<void(base::sequence_manager::TaskQueue*,
                               base::TimeTicks)>
      forward_immediate_work_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> control_task_runner_;
  ThreadSchedulerImpl* thread_scheduler_;            // NOT OWNED
  TraceableVariableController* tracing_controller_;  // NOT OWNED
  const base::TickClock* tick_clock_;                // NOT OWNED
  std::unique_ptr<ThrottledTimeDomain> time_domain_;

  CancelableClosureHolder pump_throttled_tasks_closure_;
  base::Optional<base::TimeTicks> pending_pump_throttled_tasks_runtime_;
  bool allow_throttling_;

  HashMap<BudgetPool*, std::unique_ptr<BudgetPool>> budget_pools_;

  base::WeakPtrFactory<TaskQueueThrottler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TaskQueueThrottler);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_TASK_QUEUE_THROTTLER_H_
