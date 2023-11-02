// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"

#include <memory>
#include "base/location.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

void PingPongTask(base::WaitableEvent* done_event) {
  done_event->Signal();
}

}  // namespace

TEST(BackgroundSchedulerTest, RunOnBackgroundThread) {
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<base::WaitableEvent> done_event =
      std::make_unique<base::WaitableEvent>();
  worker_pool::PostTask(
      FROM_HERE, CrossThreadBindOnce(&PingPongTask,
                                     CrossThreadUnretained(done_event.get())));
  // Test passes by not hanging on the following wait().
  done_event->Wait();
}

}  // namespace blink
