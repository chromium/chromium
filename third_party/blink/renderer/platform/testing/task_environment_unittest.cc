// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/task_environment.h"

#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"

namespace blink {

enum class SchedulerType {
  kSimple,
  kMainThread,
};

class TaskEnvironmentTestParam : public testing::TestWithParam<SchedulerType> {
 public:
  void SetUp() override {
    if (GetParam() == SchedulerType::kMainThread) {
      task_environment_.emplace(
          test::TaskEnvironment::RealMainThreadScheduler());
    } else {
      task_environment_.emplace();
    }
  }

 protected:
  absl::optional<test::TaskEnvironment> task_environment_;
};

TEST_P(TaskEnvironmentTestParam, MainThreadTaskRunner) {
  auto quit_closure = (*task_environment_)->QuitClosure();
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        Thread::MainThread()
            ->GetTaskRunner(MainThreadTaskRunnerRestrictedForTesting())
            ->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                         EXPECT_TRUE(Thread::MainThread()->IsCurrentThread());
                         std::move(quit_closure).Run();
                       }));
      }));

  (*task_environment_)->RunUntilQuit();
}

TEST_P(TaskEnvironmentTestParam, Isolate) {
  EXPECT_TRUE(task_environment_->isolate());
}

INSTANTIATE_TEST_SUITE_P(All,
                         TaskEnvironmentTestParam,
                         ::testing::Values(SchedulerType::kSimple,
                                           SchedulerType::kMainThread));

}  // namespace blink
