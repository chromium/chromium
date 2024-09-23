// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/task_environment.h"

#include <optional>

#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"

namespace blink {

enum class SchedulerType {
  kSimple,
  kMainThread,
};

class TaskEnvironmentTest : public testing::Test {
 protected:
  test::TaskEnvironment task_environment_;
};

TEST_F(TaskEnvironmentTest, MainThreadTaskRunner) {
  auto quit_closure = task_environment_.QuitClosure();
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        Thread::MainThread()
            ->GetTaskRunner(MainThreadTaskRunnerRestrictedForTesting())
            ->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                         EXPECT_TRUE(Thread::MainThread()->IsCurrentThread());
                         std::move(quit_closure).Run();
                       }));
      }));

  task_environment_.RunUntilQuit();
}

TEST_F(TaskEnvironmentTest, Isolate) {
  EXPECT_TRUE(task_environment_.isolate());
}

}  // namespace blink
