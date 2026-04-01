// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_web_scheduling_task_queue_impl.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_impl.h"

namespace blink::scheduler {

NonMainThreadWebSchedulingTaskQueueImpl::
    NonMainThreadWebSchedulingTaskQueueImpl(
        base::WeakPtr<WorkerSchedulerImpl> scheduler,
        scoped_refptr<NonMainThreadTaskQueue> task_queue)
    : scheduler_(std::move(scheduler)),
      task_runner_(
          task_queue->CreateTaskRunner(TaskType::kWebSchedulingPostedTask)),
      task_queue_(std::move(task_queue)) {}

NonMainThreadWebSchedulingTaskQueueImpl::
    ~NonMainThreadWebSchedulingTaskQueueImpl() {
  if (scheduler_) {
    scheduler_->OnWebSchedulingTaskQueueDestroyed(task_queue_.get());
  }
}

void NonMainThreadWebSchedulingTaskQueueImpl::SetPriority(
    WebSchedulingPriority priority) {
  task_queue_->SetWebSchedulingPriority(priority);
}

scoped_refptr<base::SingleThreadTaskRunner>
NonMainThreadWebSchedulingTaskQueueImpl::GetTaskRunner() {
  return task_runner_;
}

}  // namespace blink::scheduler
