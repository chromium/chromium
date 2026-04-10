// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/test/task_environment.h"

#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_helper.h"

namespace blink::test {

TaskEnvironmentWithNonMainThreadSchedulerHelper::
    ~TaskEnvironmentWithNonMainThreadSchedulerHelper() = default;

TaskEnvironmentWithNonMainThreadSchedulerHelper::
    TaskEnvironmentWithNonMainThreadSchedulerHelper(
        base::test::TaskEnvironment&& scoped_task_environment)
    : base::test::TaskEnvironment(std::move(scoped_task_environment)) {
  scheduler_helper_ = std::make_unique<scheduler::NonMainThreadSchedulerHelper>(
      sequence_manager(), nullptr, TaskType::kInternalTest);
  scheduler_helper_->AttachToCurrentThread();
  DeferredInitFromSubclass(
      scheduler_helper_->DefaultNonMainThreadTaskQueue()->GetTaskQueue());
}

}  // namespace blink::test
