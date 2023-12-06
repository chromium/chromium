// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/task_environment.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink::test {
namespace internal {
namespace {

bool g_task_environment_supported = false;

}

TaskEnvironmentImpl::~TaskEnvironmentImpl() {
  RunUntilIdle();
  if (!scheduler_) {
    Platform::UnsetMainThreadTaskRunnerForTesting();
  }
  main_thread_overrider_.reset();
  main_thread_isolate_.reset();
  if (scheduler_) {
    scheduler_->Shutdown();
  }
}

TaskEnvironmentImpl::TaskEnvironmentImpl(
    base::test::TaskEnvironment&& scoped_task_environment,
    bool real_main_thread_scheduler)
    : base::test::TaskEnvironment(std::move(scoped_task_environment)) {
  CHECK(g_task_environment_supported);
  CHECK(IsMainThread());
  if (real_main_thread_scheduler) {
    scheduler_ = std::make_unique<scheduler::MainThreadSchedulerImpl>(
        sequence_manager());
    DeferredInitFromSubclass(scheduler_->DefaultTaskRunner());
  }

  main_thread_isolate_.emplace();

  if (scheduler_) {
    main_thread_overrider_.emplace(scheduler_->CreateMainThread());
  } else {
    // When |real_main_thread_scheduler| == false, this simply relies on
    // the test suite providing a DummyWebMainThreadScheduler. We only
    // need to provide support for e.g.
    // base::SingleThreadTaskRunner::GetCurrentDefault() through this
    // TaskEnvironment.
    // Make MainThread()->GetTaskRunner() accessible from non main thread
    // later on.
    Platform::SetMainThreadTaskRunnerForTesting();
  }
}

// static
bool TaskEnvironmentImpl::IsSupported() {
  CHECK(IsMainThread());
  return g_task_environment_supported;
}

// static
void TaskEnvironmentImpl::SetSupported(bool is_supported) {
  CHECK(!g_task_environment_supported);
  g_task_environment_supported = is_supported;
}

}  // namespace internal

v8::Isolate* TaskEnvironment::isolate() {
  if (impl_) {
    return impl_->isolate();
  }
  return blink::MainThreadIsolate();
}

}  // namespace blink::test
