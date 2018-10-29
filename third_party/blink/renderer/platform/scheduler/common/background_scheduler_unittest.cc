// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/background_scheduler.h"

#include <memory>
#include "base/location.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/waitable_event.h"

namespace blink {

namespace {

void PingPongTask(WaitableEvent* done_event) {
  done_event->Signal();
}

}  // namespace

TEST(BackgroundSchedulerTest, RunOnBackgroundThread) {
  base::test::ScopedTaskEnvironment scoped_task_environment;
  std::unique_ptr<WaitableEvent> done_event = std::make_unique<WaitableEvent>();
  background_scheduler::PostOnBackgroundThread(
      FROM_HERE,
      CrossThreadBind(&PingPongTask, CrossThreadUnretained(done_event.get())));
  // Test passes by not hanging on the following wait().
  done_event->Wait();
}

}  // namespace blink
