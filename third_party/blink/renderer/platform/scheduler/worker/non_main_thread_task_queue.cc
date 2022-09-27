// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_task_queue.h"

#include "base/bind.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_base.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;

NonMainThreadTaskQueue::NonMainThreadTaskQueue(
    std::unique_ptr<base::sequence_manager::internal::TaskQueueImpl> impl,
    const TaskQueue::Spec& spec,
    NonMainThreadSchedulerBase* non_main_thread_scheduler,
    bool can_be_throttled)
    : task_queue_(base::MakeRefCounted<TaskQueue>(std::move(impl), spec)),
      non_main_thread_scheduler_(non_main_thread_scheduler) {
  // Throttling needs |should_notify_observers| to get task timing.
  DCHECK(!can_be_throttled || spec.should_notify_observers)
      << "Throttled queue is not supported with |!should_notify_observers|";
  if (task_queue_->HasImpl() && spec.should_notify_observers) {
    if (can_be_throttled) {
      throttler_.emplace(task_queue_.get(),
                         non_main_thread_scheduler->GetTickClock());
    }
    // TaskQueueImpl may be null for tests.
    task_queue_->SetOnTaskCompletedHandler(base::BindRepeating(
        &NonMainThreadTaskQueue::OnTaskCompleted, base::Unretained(this)));
  }
}

NonMainThreadTaskQueue::~NonMainThreadTaskQueue() = default;

void NonMainThreadTaskQueue::ShutdownTaskQueue() {
  non_main_thread_scheduler_ = nullptr;
  throttler_.reset();
  task_queue_->ShutdownTaskQueue();
}

void NonMainThreadTaskQueue::OnTaskCompleted(
    const base::sequence_manager::Task& task,
    TaskQueue::TaskTiming* task_timing,
    base::LazyNow* lazy_now) {
  // |non_main_thread_scheduler_| can be nullptr in tests.
  if (non_main_thread_scheduler_) {
    non_main_thread_scheduler_->OnTaskCompleted(this, task, task_timing,
                                                lazy_now);
  }
}

void NonMainThreadTaskQueue::AddToBudgetPool(base::TimeTicks now,
                                             BudgetPool* pool) {
  pool->AddThrottler(now, &throttler_.value());
}

void NonMainThreadTaskQueue::RemoveFromBudgetPool(base::TimeTicks now,
                                                  BudgetPool* pool) {
  pool->RemoveThrottler(now, &throttler_.value());
}

void NonMainThreadTaskQueue::IncreaseThrottleRefCount() {
  throttler_->IncreaseThrottleRefCount();
}

void NonMainThreadTaskQueue::DecreaseThrottleRefCount() {
  throttler_->DecreaseThrottleRefCount();
}

void NonMainThreadTaskQueue::OnTaskRunTimeReported(
    TaskQueue::TaskTiming* task_timing) {
  if (throttler_.has_value()) {
    throttler_->OnTaskRunTimeReported(task_timing->start_time(),
                                      task_timing->end_time());
  }
}

void NonMainThreadTaskQueue::SetWebSchedulingPriority(
    WebSchedulingPriority priority) {
  if (web_scheduling_priority_ == priority)
    return;
  web_scheduling_priority_ = priority;
  OnWebSchedulingPriorityChanged();
}

void NonMainThreadTaskQueue::OnWebSchedulingPriorityChanged() {
  DCHECK(web_scheduling_priority_);
  switch (web_scheduling_priority_.value()) {
    case WebSchedulingPriority::kUserBlockingPriority:
      task_queue_->SetQueuePriority(TaskQueue::QueuePriority::kHighPriority);
      return;
    case WebSchedulingPriority::kUserVisiblePriority:
      task_queue_->SetQueuePriority(TaskQueue::QueuePriority::kNormalPriority);
      return;
    case WebSchedulingPriority::kBackgroundPriority:
      task_queue_->SetQueuePriority(TaskQueue::QueuePriority::kLowPriority);
      return;
  }
}

}  // namespace scheduler
}  // namespace blink
