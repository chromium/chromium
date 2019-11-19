// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_helper.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_task_queue.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;

NonMainThreadSchedulerHelper::NonMainThreadSchedulerHelper(
    base::sequence_manager::SequenceManager* sequence_manager,
    NonMainThreadSchedulerImpl* non_main_thread_scheduler,
    TaskType default_task_type)
    : SchedulerHelper(sequence_manager),
      non_main_thread_scheduler_(non_main_thread_scheduler),
      default_task_queue_(NewTaskQueue(TaskQueue::Spec("subthread_default_tq")
                                           .SetShouldMonitorQuiescence(true))),
      control_task_queue_(NewTaskQueue(TaskQueue::Spec("subthread_control_tq")
                                           .SetShouldNotifyObservers(false))) {
  InitDefaultQueues(default_task_queue_, control_task_queue_,
                    default_task_type);
}

NonMainThreadSchedulerHelper::~NonMainThreadSchedulerHelper() {
  control_task_queue_->ShutdownTaskQueue();
  default_task_queue_->ShutdownTaskQueue();
}

scoped_refptr<NonMainThreadTaskQueue>
NonMainThreadSchedulerHelper::DefaultNonMainThreadTaskQueue() {
  return default_task_queue_;
}

scoped_refptr<TaskQueue> NonMainThreadSchedulerHelper::DefaultTaskQueue() {
  return default_task_queue_;
}

scoped_refptr<NonMainThreadTaskQueue>
NonMainThreadSchedulerHelper::ControlNonMainThreadTaskQueue() {
  return control_task_queue_;
}

scoped_refptr<TaskQueue> NonMainThreadSchedulerHelper::ControlTaskQueue() {
  return control_task_queue_;
}

scoped_refptr<NonMainThreadTaskQueue>
NonMainThreadSchedulerHelper::NewTaskQueue(const TaskQueue::Spec& spec) {
  return sequence_manager_->CreateTaskQueueWithType<NonMainThreadTaskQueue>(
      spec, non_main_thread_scheduler_);
}

void NonMainThreadSchedulerHelper::ShutdownAllQueues() {
  default_task_queue_->ShutdownTaskQueue();
  control_task_queue_->ShutdownTaskQueue();
}

}  // namespace scheduler
}  // namespace blink
