// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/task_environment.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/scheduler/task_attribution_tracker_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink::test {
namespace internal {
namespace {

bool g_task_environment_supported = false;

}

TaskEnvironmentImpl::~TaskEnvironmentImpl() {
  RunUntilIdle();
  main_thread_overrider_.reset();
  main_thread_isolate_.reset();
  scheduler_->Shutdown();
}

TaskEnvironmentImpl::TaskEnvironmentImpl(
    base::test::TaskEnvironment&& scoped_task_environment)
    : base::test::TaskEnvironment(std::move(scoped_task_environment)) {
  CHECK(g_task_environment_supported);
  CHECK(IsMainThread());
  scheduler_ =
      std::make_unique<scheduler::MainThreadSchedulerImpl>(sequence_manager());
  DeferredInitFromSubclass(scheduler_->DefaultTaskRunner());

  main_thread_isolate_.emplace();

  main_thread_overrider_.emplace(scheduler_->CreateMainThread());
  ThreadScheduler::Current()->InitializeTaskAttributionTracker(
      std::make_unique<scheduler::TaskAttributionTrackerImpl>());
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
  return Thread::MainThread()->Scheduler()->ToMainThreadScheduler()->Isolate();
}

}  // namespace blink::test
