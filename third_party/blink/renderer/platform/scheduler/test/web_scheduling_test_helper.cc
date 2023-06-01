// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/test/web_scheduling_test_helper.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_queue_type.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace {

void AppendToVectorTestTask(Vector<String>* vector, String value) {
  vector->push_back(std::move(value));
}

}  // namespace

namespace blink::scheduler {

WebSchedulingTestHelper::WebSchedulingTestHelper(Delegate& delegate)
    : delegate_(delegate) {
  FrameOrWorkerScheduler& frame_or_worker_scheduler =
      delegate_->GetFrameOrWorkerScheduler();
  for (int i = 0; i <= static_cast<int>(WebSchedulingPriority::kLastPriority);
       i++) {
    WebSchedulingPriority priority = static_cast<WebSchedulingPriority>(i);
    task_queues_.push_back(
        frame_or_worker_scheduler.CreateWebSchedulingTaskQueue(
            WebSchedulingQueueType::kTaskQueue, priority));
    continuation_task_queues_.push_back(
        frame_or_worker_scheduler.CreateWebSchedulingTaskQueue(
            WebSchedulingQueueType::kContinuationQueue, priority));
  }
}

WebSchedulingTestHelper::~WebSchedulingTestHelper() = default;

WebSchedulingTaskQueue* WebSchedulingTestHelper::GetWebSchedulingTaskQueue(
    WebSchedulingQueueType queue_type,
    WebSchedulingPriority priority) {
  switch (queue_type) {
    case WebSchedulingQueueType::kTaskQueue:
      return task_queues_[static_cast<wtf_size_t>(priority)].get();
    case WebSchedulingQueueType::kContinuationQueue:
      return continuation_task_queues_[static_cast<wtf_size_t>(priority)].get();
  }
}

void WebSchedulingTestHelper::PostTestTasks(
    Vector<String>* run_order,
    const Vector<TestTaskSpecEntry>& test_spec) {
  for (const auto& entry : test_spec) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner;
    if (absl::holds_alternative<WebSchedulingParams>(entry.type_info)) {
      WebSchedulingParams params =
          absl::get<WebSchedulingParams>(entry.type_info);
      task_runner =
          GetWebSchedulingTaskQueue(params.queue_type, params.priority)
              ->GetTaskRunner();
    } else {
      task_runner =
          delegate_->GetTaskRunner(absl::get<TaskType>(entry.type_info));
    }
    task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AppendToVectorTestTask, run_order, entry.descriptor),
        entry.delay);
  }
}

}  // namespace blink::scheduler
