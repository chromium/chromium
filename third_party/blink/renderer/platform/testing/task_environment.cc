// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/task_environment.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink::test {

TaskEnvironment::~TaskEnvironment() {
  // Run a full GC before resetting the main thread overrider. This ensures that
  // we can properly clean up objects like PerformanceMonitor that need to call
  // MainThreadImpl::RemoveTaskTimeObserver().
  ThreadState::Current()->CollectAllGarbageForTesting();
  RunUntilIdle();

  main_thread_overrider_.reset();
  main_thread_isolate_.reset();
  scheduler_->Shutdown();
}

TaskEnvironment::TaskEnvironment(
    base::test::TaskEnvironment&& scoped_task_environment)
    : base::test::TaskEnvironment(std::move(scoped_task_environment)) {
  CHECK(IsMainThread());
  scheduler_ =
      std::make_unique<scheduler::MainThreadSchedulerImpl>(sequence_manager());
  DeferredInitFromSubclass(scheduler_->DefaultTaskRunner());

  main_thread_isolate_.emplace();

  main_thread_overrider_.emplace(scheduler_->CreateMainThread());
}

}  // namespace blink::test
