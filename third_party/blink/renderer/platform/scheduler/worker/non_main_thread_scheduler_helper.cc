// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_helper.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_task_queue.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::QueueName;
using base::sequence_manager::TaskQueue;

NonMainThreadSchedulerHelper::NonMainThreadSchedulerHelper(
    base::sequence_manager::SequenceManager* sequence_manager,
    NonMainThreadSchedulerBase* non_main_thread_scheduler,
    TaskType default_task_type)
    : SchedulerHelper(sequence_manager),
      non_main_thread_scheduler_(non_main_thread_scheduler),
      default_task_queue_(
          NewTaskQueueInternal(TaskQueue::Spec(QueueName::SUBTHREAD_DEFAULT_TQ)
                                   .SetShouldMonitorQuiescence(true))),
      input_task_queue_(
          NewTaskQueueInternal(TaskQueue::Spec(QueueName::SUBTHREAD_INPUT_TQ))),
      control_task_queue_(
          NewTaskQueue(TaskQueue::Spec(QueueName::SUBTHREAD_CONTROL_TQ)
                           .SetShouldNotifyObservers(false))) {
  control_task_queue_->SetQueuePriority(TaskPriority::kControlPriority);
  input_task_queue_->SetQueuePriority(TaskPriority::kHighestPriority);

  InitDefaultTaskRunner(
      default_task_queue_->CreateTaskRunner(default_task_type));
}

NonMainThreadSchedulerHelper::~NonMainThreadSchedulerHelper() {
  control_task_queue_->ShutdownTaskQueue();
  default_task_queue_->ShutdownTaskQueue();
}

scoped_refptr<NonMainThreadTaskQueue>
NonMainThreadSchedulerHelper::DefaultNonMainThreadTaskQueue() {
  return default_task_queue_;
}

const scoped_refptr<base::SingleThreadTaskRunner>&
NonMainThreadSchedulerHelper::InputTaskRunner() {
  return input_task_queue_->GetTaskRunnerWithDefaultTaskType();
}

scoped_refptr<NonMainThreadTaskQueue>
NonMainThreadSchedulerHelper::ControlNonMainThreadTaskQueue() {
  return control_task_queue_;
}

const scoped_refptr<base::SingleThreadTaskRunner>&
NonMainThreadSchedulerHelper::ControlTaskRunner() {
  return control_task_queue_->GetTaskRunnerWithDefaultTaskType();
}

scoped_refptr<NonMainThreadTaskQueue>
NonMainThreadSchedulerHelper::NewTaskQueue(
    const TaskQueue::Spec& spec,
    NonMainThreadTaskQueue::QueueCreationParams params) {
  DCHECK(default_task_queue_);
  return base::MakeRefCounted<NonMainThreadTaskQueue>(
      *sequence_manager_, spec, non_main_thread_scheduler_, params,
      default_task_queue_->GetTaskRunnerWithDefaultTaskType());
}

scoped_refptr<NonMainThreadTaskQueue>
NonMainThreadSchedulerHelper::NewTaskQueueInternal(
    const TaskQueue::Spec& spec,
    NonMainThreadTaskQueue::QueueCreationParams params) {
  return base::MakeRefCounted<NonMainThreadTaskQueue>(
      *sequence_manager_, spec, non_main_thread_scheduler_, params, nullptr);
}

void NonMainThreadSchedulerHelper::ShutdownAllQueues() {
  default_task_queue_->ShutdownTaskQueue();
  control_task_queue_->ShutdownTaskQueue();
}

}  // namespace scheduler
}  // namespace blink
