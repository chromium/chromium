// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/scheduler/net_task_scheduler.h"

#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/features.h"
#include "net/base/request_priority.h"
#include "net/base/scheduler/net_task_priority.h"
#include "net/base/task/task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

using StrictMockTask =
    testing::StrictMock<base::MockCallback<base::RepeatingCallback<void()>>>;

class TestNetTaskEnvironment : public base::test::TaskEnvironment {
 public:
  using ValidTraits = base::test::TaskEnvironment::ValidTraits;

  template <typename... Traits>
    requires base::trait_helpers::AreValidTraits<ValidTraits, Traits...>
  explicit TestNetTaskEnvironment(Traits... traits)
      : TestNetTaskEnvironment(CreateTaskEnvironmentWithPriorities(
            CreateNetTaskPrioritySettings(),
            SubclassCreatesDefaultTaskRunner{},
            traits...)) {}

  ~TestNetTaskEnvironment() override = default;

  NetTaskScheduler* scheduler() { return scheduler_.get(); }

 private:
  explicit TestNetTaskEnvironment(
      base::test::TaskEnvironment&& scoped_task_environment)
      : base::test::TaskEnvironment(std::move(scoped_task_environment)) {
    scheduler_ = NetTaskScheduler::CreateForTesting(sequence_manager());
    DeferredInitFromSubclass(scheduler_->GetDefaultTaskQueue());
  }

  std::unique_ptr<NetTaskScheduler> scheduler_;
};

// Test fixture for NetTaskScheduler. This is a parameterized test
// that runs with the `kNetTaskPerPriorityTaskQueues` feature enabled and
// disabled.
class NetTaskSchedulerTest : public testing::TestWithParam<bool> {
 protected:
  NetTaskSchedulerTest() {
    if (IsPerPriorityQueuesEnabled()) {
      feature_list_.InitAndEnableFeature(
          features::kNetworkServicePerPriorityTaskQueues);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kNetworkServicePerPriorityTaskQueues);
    }
  }

  NetTaskScheduler* scheduler() { return task_environment_.scheduler(); }

  bool IsPerPriorityQueuesEnabled() const { return GetParam(); }

 private:
  TestNetTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that tasks posted to the default task runner provided by the scheduler
// are executed in order. Also verifies that the scheduler sets the current
// thread's default task runner.
TEST_P(NetTaskSchedulerTest, DefaultQueuePostsTasksInOrder) {
  scoped_refptr<base::SingleThreadTaskRunner> tq =
      scheduler()->GetDefaultTaskRunner();
  EXPECT_EQ(tq.get(), base::SingleThreadTaskRunner::GetCurrentDefault());

  StrictMockTask task_1;
  StrictMockTask task_2;

  testing::InSequence s;
  EXPECT_CALL(task_1, Run);
  EXPECT_CALL(task_2, Run);

  base::RunLoop run_loop;
  tq->PostTask(FROM_HERE, task_1.Get());
  tq->PostTask(FROM_HERE, task_2.Get());
  tq->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Tests that tasks posted to different priority queues (default and highest
// priority) are executed according to their priority, with highest priority
// tasks running before default priority tasks.
TEST_P(NetTaskSchedulerTest, MultipleQueuesHighPriorityTaskRunsFirst) {
  scoped_refptr<base::SingleThreadTaskRunner> tq1 =
      scheduler()->GetTaskRunner(RequestPriority::DEFAULT_PRIORITY);
  scoped_refptr<base::SingleThreadTaskRunner> tq2 =
      scheduler()->GetTaskRunner(RequestPriority::HIGHEST);

  StrictMockTask task_1;
  StrictMockTask task_2;

  testing::InSequence s;
  // High priority task runs at first.
  EXPECT_CALL(task_2, Run);
  // Default priority task runs after a high priority task runs.
  EXPECT_CALL(task_1, Run);

  base::RunLoop run_loop;
  tq1->PostTask(FROM_HERE, task_1.Get());
  tq2->PostTask(FROM_HERE, task_2.Get());
  tq1->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Tests that tasks posted to all different priority queues are executed
// according to their priority.
TEST_P(NetTaskSchedulerTest, PostToAllPriorites) {
  StrictMockTask task_1;
  StrictMockTask task_2;
  StrictMockTask task_3;
  StrictMockTask task_4;
  StrictMockTask task_5;
  StrictMockTask task_6;

  testing::InSequence s;
  EXPECT_CALL(task_6, Run);
  EXPECT_CALL(task_5, Run);
  EXPECT_CALL(task_4, Run);
  EXPECT_CALL(task_3, Run);
  EXPECT_CALL(task_2, Run);
  EXPECT_CALL(task_1, Run);

  base::RunLoop run_loop;

  scheduler()
      ->GetTaskRunner(RequestPriority::THROTTLED)
      ->PostTask(FROM_HERE, task_1.Get());
  scheduler()
      ->GetTaskRunner(RequestPriority::IDLE)
      ->PostTask(FROM_HERE, task_2.Get());
  scheduler()
      ->GetTaskRunner(RequestPriority::LOWEST)
      ->PostTask(FROM_HERE, task_3.Get());
  scheduler()
      ->GetTaskRunner(RequestPriority::LOW)
      ->PostTask(FROM_HERE, task_4.Get());
  scheduler()
      ->GetTaskRunner(RequestPriority::MEDIUM)
      ->PostTask(FROM_HERE, task_5.Get());
  scheduler()
      ->GetTaskRunner(RequestPriority::HIGHEST)
      ->PostTask(FROM_HERE, task_6.Get());

  scheduler()
      ->GetTaskRunner(RequestPriority::THROTTLED)
      ->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Tests the `SetUpNetTaskRunners`.
// Verifies that after calling `SetupNetTaskRunners`, `net::GetTaskRunner`
// returns the appropriate task runners managed by the scheduler for all
// priority levels.
TEST_P(NetTaskSchedulerTest,
       SetUpNetTaskRunnersIntegratesWithNetGetTaskRunner) {
  scheduler()->SetUpNetTaskRunnersForTesting();

  if (IsPerPriorityQueuesEnabled()) {
    for (int i = 0; i < NUM_PRIORITIES; ++i) {
      RequestPriority priority = static_cast<RequestPriority>(i);
      EXPECT_EQ(GetTaskRunner(priority).get(),
                scheduler()->GetTaskRunner(priority).get());
    }
  } else {
    // When per-priority queues are disabled, all priorities except HIGHEST
    // should map to the default task queue.
    EXPECT_NE(scheduler()->GetTaskRunner(RequestPriority::HIGHEST).get(),
              scheduler()->GetDefaultTaskRunner().get());
    for (int i = 0; i < NUM_PRIORITIES; ++i) {
      RequestPriority priority = static_cast<RequestPriority>(i);
      if (priority == RequestPriority::HIGHEST) {
        EXPECT_EQ(GetTaskRunner(priority).get(),
                  scheduler()->GetTaskRunner(priority).get());
      } else {
        EXPECT_EQ(GetTaskRunner(priority).get(),
                  scheduler()->GetDefaultTaskRunner().get());
      }
    }
  }

  // Also, verify that the default task runner is set up correctly.
  EXPECT_EQ(scheduler()->GetDefaultTaskRunner().get(),
            base::SingleThreadTaskRunner::GetCurrentDefault().get());
}

INSTANTIATE_TEST_SUITE_P(All, NetTaskSchedulerTest, testing::Bool());

}  // namespace
}  // namespace net
