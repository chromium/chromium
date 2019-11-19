// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/web_scheduling_task_queue_impl.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

WebSchedulingTaskQueueImpl::WebSchedulingTaskQueueImpl(
    WebSchedulingPriority priority,
    MainThreadTaskQueue* task_queue)
    : priority_(priority),
      task_runner_(
          task_queue->CreateTaskRunner(TaskType::kExperimentalWebScheduling)) {}

scoped_refptr<base::SingleThreadTaskRunner>
WebSchedulingTaskQueueImpl::GetTaskRunner() {
  return task_runner_;
}

}  // namespace scheduler
}  // namespace blink
