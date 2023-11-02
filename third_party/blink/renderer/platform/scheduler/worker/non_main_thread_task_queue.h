// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_NON_MAIN_THREAD_TASK_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_NON_MAIN_THREAD_TASK_QUEUE_H_

#include "base/task/common/lazy_now.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"

namespace blink {
namespace scheduler {

using TaskQueue = base::sequence_manager::TaskQueue;

class NonMainThreadSchedulerBase;

class PLATFORM_EXPORT NonMainThreadTaskQueue
    : public base::RefCountedThreadSafe<NonMainThreadTaskQueue> {
 public:
  // TODO(kraynov): Consider options to remove TaskQueueImpl reference here.
  NonMainThreadTaskQueue(
      std::unique_ptr<base::sequence_manager::internal::TaskQueueImpl> impl,
      const TaskQueue::Spec& spec,
      NonMainThreadSchedulerBase* non_main_thread_scheduler,
      bool can_be_throttled);
  ~NonMainThreadTaskQueue();

  void OnTaskCompleted(
      const base::sequence_manager::Task& task,
      base::sequence_manager::TaskQueue::TaskTiming* task_timing,
      base::LazyNow* lazy_now);

  scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner(
      TaskType task_type) {
    return task_queue_->CreateTaskRunner(static_cast<int>(task_type));
  }

  bool IsThrottled() const { return throttler_->IsThrottled(); }

  // Methods for setting and resetting budget pools for this task queue.
  // Note that a task queue can be in multiple budget pools so a pool must
  // be specified when removing.
  void AddToBudgetPool(base::TimeTicks now, BudgetPool* pool);
  void RemoveFromBudgetPool(base::TimeTicks now, BudgetPool* pool);

  void IncreaseThrottleRefCount();
  void DecreaseThrottleRefCount();

  void SetQueuePriority(TaskQueue::QueuePriority priority) {
    task_queue_->SetQueuePriority(priority);
  }
  TaskQueue::QueuePriority GetQueuePriority() const {
    return task_queue_->GetQueuePriority();
  }

  std::unique_ptr<TaskQueue::QueueEnabledVoter> CreateQueueEnabledVoter() {
    return task_queue_->CreateQueueEnabledVoter();
  }

  void ShutdownTaskQueue();

  // This method returns the default task runner with task type kTaskTypeNone
  // and is mostly used for tests. For most use cases, you'll want a more
  // specific task runner and should use the 'CreateTaskRunner' method and pass
  // the desired task type.
  const scoped_refptr<base::SingleThreadTaskRunner>&
  GetTaskRunnerWithDefaultTaskType() const {
    return task_queue_->task_runner();
  }

  void SetWebSchedulingPriority(WebSchedulingPriority priority);

  void OnTaskRunTimeReported(TaskQueue::TaskTiming* task_timing);

  // TODO(crbug.com/1143007): Improve MTTQ API surface so that we no longer
  // need to expose the raw pointer to the queue.
  TaskQueue* GetTaskQueue() { return task_queue_.get(); }

  // This method returns the default task runner with task type kTaskTypeNone
  // and is mostly used for tests. For most use cases, you'll want a more
  // specific task runner and should use the 'CreateTaskRunner' method and pass
  // the desired task type.
  const scoped_refptr<base::SingleThreadTaskRunner>&
  GetTaskRunnerWithDefaultTaskType() {
    return task_queue_->task_runner();
  }

 private:
  void OnWebSchedulingPriorityChanged();

  scoped_refptr<TaskQueue> task_queue_;
  absl::optional<TaskQueueThrottler> throttler_;

  // Not owned.
  NonMainThreadSchedulerBase* non_main_thread_scheduler_;

  // |web_scheduling_priority_| is the priority of the task queue within the web
  // scheduling API. This priority is used to determine the task queue priority.
  absl::optional<WebSchedulingPriority> web_scheduling_priority_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_NON_MAIN_THREAD_TASK_QUEUE_H_
