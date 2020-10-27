// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/widget_scheduler.h"

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

WidgetScheduler::WidgetScheduler(
    MainThreadSchedulerImpl* main_thread_scheduler) {
  input_task_queue_ = main_thread_scheduler->NewTaskQueue(
      MainThreadTaskQueue::QueueCreationParams(
          MainThreadTaskQueue::QueueType::kInput)
          .SetShouldMonitorQuiescence(true)
          .SetPrioritisationType(
              MainThreadTaskQueue::QueueTraits::PrioritisationType::kInput));
  input_task_runner_ =
      input_task_queue_->CreateTaskRunner(TaskType::kMainThreadTaskQueueInput);
  input_task_queue_enabled_voter_ =
      input_task_queue_->GetTaskQueue()->CreateQueueEnabledVoter();
}

WidgetScheduler::~WidgetScheduler() {
  input_task_queue_->ShutdownTaskQueue();
}

scoped_refptr<base::SingleThreadTaskRunner> WidgetScheduler::InputTaskRunner() {
  return input_task_runner_;
}

}  // namespace scheduler
}  // namespace blink
