// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/main_thread_isolate.h"

#include "base/run_loop.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink::test {

MainThreadIsolate::MainThreadIsolate() {
  isolate_ = CreateMainThreadIsolate();
}

MainThreadIsolate::~MainThreadIsolate() {
  CHECK_NE(nullptr, isolate_);
  MemoryCache::Get()->EvictResources();
  isolate()->ClearCachesForTesting();
  V8PerIsolateData::From(isolate())->ClearScriptRegexpContext();
  ThreadState::Current()->CollectAllGarbageForTesting();

  ThreadScheduler::Current()->SetV8Isolate(nullptr);
  V8PerIsolateData::WillBeDestroyed(isolate());
  v8::Isolate* isolate = isolate_.get();
  isolate_ = nullptr;
  V8PerIsolateData::Destroy(isolate);
}

}  // namespace blink::test
