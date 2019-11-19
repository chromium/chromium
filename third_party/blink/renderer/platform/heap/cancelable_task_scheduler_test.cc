// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/cancelable_task_scheduler.h"

#include <atomic>

#include "base/memory/scoped_refptr.h"
#include "base/task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class ParallelTaskRunner : public base::TaskRunner {
 public:
  bool PostDelayedTask(const base::Location& location,
                       base::OnceClosure task,
                       base::TimeDelta) override {
    worker_pool::PostTask(location, WTF::CrossThreadBindOnce(std::move(task)));
    return true;
  }

  bool RunsTasksInCurrentSequence() const override { return false; }

  void RunUntilIdle() {}
};

template <class Runner>
class CancelableTaskSchedulerTest : public TestSupportingGC {
 public:
  using Task = CancelableTaskScheduler::Task;

  void ScheduleTask(Task callback) {
    scheduler_.ScheduleTask(std::move(callback));
  }

  void RunTaskRunner() { task_runner_->RunUntilIdle(); }
  size_t CancelAndWait() { return scheduler_.CancelAndWait(); }

  size_t NumberOfRegisteredTasks() const {
    return scheduler_.NumberOfTasksForTesting();
  }

 private:
  scoped_refptr<Runner> task_runner_ = base::MakeRefCounted<Runner>();
  CancelableTaskScheduler scheduler_{task_runner_};
};

using RunnerTypes =
    ::testing::Types<scheduler::FakeTaskRunner, ParallelTaskRunner>;
TYPED_TEST_SUITE(CancelableTaskSchedulerTest, RunnerTypes);

TYPED_TEST(CancelableTaskSchedulerTest, EmptyCancelTasks) {
  const size_t cancelled = this->CancelAndWait();
  EXPECT_EQ(0u, cancelled);
  EXPECT_EQ(0u, this->NumberOfRegisteredTasks());
}

TYPED_TEST(CancelableTaskSchedulerTest, RunAndCancelTasks) {
  static constexpr size_t kNumberOfTasks = 10u;

  const auto callback = [](std::atomic<int>* i) { ++(*i); };
  std::atomic<int> var{0};

  for (size_t i = 0; i < kNumberOfTasks; ++i) {
    this->ScheduleTask(
        WTF::CrossThreadBindOnce(callback, WTF::CrossThreadUnretained(&var)));
    EXPECT_GE(i + 1, this->NumberOfRegisteredTasks());
  }

  this->RunTaskRunner();
  // Tasks will remove themselves after running
  EXPECT_LE(0u, this->NumberOfRegisteredTasks());

  const size_t cancelled = this->CancelAndWait();
  EXPECT_EQ(0u, this->NumberOfRegisteredTasks());
  EXPECT_EQ(kNumberOfTasks, var + cancelled);
}

TEST(CancelableTaskSchedulerTest, RemoveTasksFromQueue) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  CancelableTaskScheduler scheduler{task_runner};
  int var = 0;
  scheduler.ScheduleTask(WTF::CrossThreadBindOnce(
      [](int* var) { ++(*var); }, WTF::CrossThreadUnretained(&var)));
  auto tasks = task_runner->TakePendingTasksForTesting();
  // Clearing the task queue should destroy all cancelable closures, which in
  // turn will notify CancelableTaskScheduler to remove corresponding tasks.
  tasks.clear();
  EXPECT_EQ(0, var);
}

}  // namespace blink
