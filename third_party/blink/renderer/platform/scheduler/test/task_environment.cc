// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/test/task_environment.h"

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink::test {

TaskEnvironmentWithMainThreadScheduler::
    ~TaskEnvironmentWithMainThreadScheduler() {
  if (scheduler_) {
    scheduler_->Shutdown();
  }
}

TaskEnvironmentWithMainThreadScheduler::TaskEnvironmentWithMainThreadScheduler(
    base::test::TaskEnvironment&& scoped_task_environment)
    : base::test::TaskEnvironment(std::move(scoped_task_environment)) {
  CHECK(IsMainThread());
  scheduler_ =
      std::make_unique<scheduler::MainThreadSchedulerImpl>(sequence_manager());
  DeferredInitFromSubclass(scheduler_->DefaultTaskRunner());
}

}  // namespace blink::test
