// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/task_environment.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink::test {

TaskEnvironment::~TaskEnvironment() {
  // Evict MemoryCache resources (e.g., cached data URIs keeping
  // `IsolatedSVGDocumentHost`s alive) and run a full GC before `RunUntilIdle()`
  // so unreachable objects from the test run their prefinalizers/destructors
  // (e.g., cancelling Mojo watchers) before pending tasks are drained.
  // Sweeping an `SVGImage` posts a deferred
  // `IsolatedSVGDocumentHost::Shutdown()` task (which may in turn release
  // nested `SVGImage`s), so keep draining tasks, evicting cached resources, and
  // collecting garbage in a loop until the main thread stays idle before
  // resetting `main_thread_overrider_` and `main_thread_isolate_`.
  MemoryCache::Get()->EvictResources();
  ThreadState::Current()->CollectAllGarbageForTesting();
  do {
    RunUntilIdle();
    MemoryCache::Get()->EvictResources();
    ThreadState::Current()->CollectAllGarbageForTesting();
  } while (!MainThreadIsIdle());

  // Since `SimpleFontData` is associated with `v8::isolate`, invalidate
  // `FontCache` after draining cleanup tasks (such as deferred
  // `IsolatedSVGDocumentHost::Shutdown()`) so `main_thread_isolate_.reset()`'s
  // GC destroys any `SimpleFontData` before the isolate is torn down.
  FontCache::Get().Invalidate();

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
  DeferredInitFromSubclass(scheduler_->DefaultTaskQueue()->GetTaskQueue());

  main_thread_isolate_.emplace();

  main_thread_overrider_.emplace(scheduler_->CreateMainThread());
}

}  // namespace blink::test
