// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/scoped_main_thread_overrider.h"

#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "v8/include/v8-isolate.h"

namespace blink {

ScopedMainThreadOverrider::ScopedMainThreadOverrider(
    std::unique_ptr<MainThread> main_thread)
    : original_main_thread_(MainThread::SetMainThread(std::move(main_thread))) {
  // TODO(dtapuska): Remove once each AgentSchedulingGroup has their own
  // isolate.
  if (auto* scheduler =
          original_main_thread_->Scheduler()->ToMainThreadScheduler()) {
    Thread::MainThread()->Scheduler()->SetV8Isolate(scheduler->Isolate());
  }
}

ScopedMainThreadOverrider::~ScopedMainThreadOverrider() {
  MainThread::SetMainThread(std::move(original_main_thread_));
}

}  // namespace blink
