// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_helper.h"

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;

MainThreadSchedulerHelper::MainThreadSchedulerHelper(
    base::sequence_manager::SequenceManager* sequence_manager,
    MainThreadSchedulerImpl* main_thread_scheduler)
    : SchedulerHelper(sequence_manager),
      main_thread_scheduler_(main_thread_scheduler),
      // TODO(hajimehoshi): Forbid V8 execution at |default_task_queue_|
      // (crbug.com/870606).
      default_task_queue_(
          NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
                           MainThreadTaskQueue::QueueType::kDefault)
                           .SetShouldMonitorQuiescence(true))),
      control_task_queue_(
          NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
                           MainThreadTaskQueue::QueueType::kControl)
                           .SetShouldNotifyObservers(false))) {
  InitDefaultQueues(default_task_queue_, control_task_queue_,
                    TaskType::kMainThreadTaskQueueDefault);
  sequence_manager_->EnableCrashKeys("blink_scheduler_async_stack");
}

MainThreadSchedulerHelper::~MainThreadSchedulerHelper() {
  control_task_queue_->ShutdownTaskQueue();
  default_task_queue_->ShutdownTaskQueue();
}

scoped_refptr<MainThreadTaskQueue>
MainThreadSchedulerHelper::DefaultMainThreadTaskQueue() {
  return default_task_queue_;
}

scoped_refptr<TaskQueue> MainThreadSchedulerHelper::DefaultTaskQueue() {
  return default_task_queue_;
}

scoped_refptr<MainThreadTaskQueue>
MainThreadSchedulerHelper::ControlMainThreadTaskQueue() {
  return control_task_queue_;
}

scoped_refptr<TaskQueue> MainThreadSchedulerHelper::ControlTaskQueue() {
  return control_task_queue_;
}

scoped_refptr<base::SingleThreadTaskRunner>
MainThreadSchedulerHelper::DeprecatedDefaultTaskRunner() {
  // TODO(hajimehoshi): Introduce a different task queue from the default task
  // queue and return the task runner created from it.
  return DefaultTaskRunner();
}

scoped_refptr<MainThreadTaskQueue> MainThreadSchedulerHelper::NewTaskQueue(
    const MainThreadTaskQueue::QueueCreationParams& params) {
  scoped_refptr<MainThreadTaskQueue> task_queue =
      sequence_manager_->CreateTaskQueueWithType<MainThreadTaskQueue>(
          params.spec, params, main_thread_scheduler_);
  if (params.fixed_priority)
    task_queue->SetQueuePriority(params.fixed_priority.value());
  return task_queue;
}

void MainThreadSchedulerHelper::ShutdownAllQueues() {
  default_task_queue_->ShutdownTaskQueue();
  control_task_queue_->ShutdownTaskQueue();
}

}  // namespace scheduler
}  // namespace blink
