// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/scheduler/network_service_task_scheduler.h"

#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/request_priority.h"
#include "net/base/task/task_runner.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_service_task_priority.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

using StrictMockTask =
    testing::StrictMock<base::MockCallback<base::RepeatingCallback<void()>>>;

class NetworkServiceTaskEnvironment : public base::test::TaskEnvironment {
 public:
  using ValidTraits = base::test::TaskEnvironment::ValidTraits;

  template <typename... Traits>
    requires base::trait_helpers::AreValidTraits<ValidTraits, Traits...>
  explicit NetworkServiceTaskEnvironment(Traits... traits)
      : NetworkServiceTaskEnvironment(CreateTaskEnvironmentWithPriorities(
            CreateNetworkServiceTaskPrioritySettings(),
            SubclassCreatesDefaultTaskRunner{},
            traits...)) {}

  ~NetworkServiceTaskEnvironment() override = default;

  NetworkServiceTaskScheduler* scheduler() { return scheduler_.get(); }

 private:
  explicit NetworkServiceTaskEnvironment(
      base::test::TaskEnvironment&& scoped_task_environment)
      : base::test::TaskEnvironment(std::move(scoped_task_environment)) {
    scheduler_ =
        NetworkServiceTaskScheduler::CreateForTesting(sequence_manager());
    DeferredInitFromSubclass(scheduler_->GetDefaultTaskQueue());
  }

  std::unique_ptr<NetworkServiceTaskScheduler> scheduler_;
};

// Test fixture for NetworkServiceTaskScheduler. This is a parameterized test
// that runs with the `kNetworkServicePerPriorityTaskQueues` feature enabled and
// disabled.
class NetworkServiceTaskSchedulerTest : public testing::TestWithParam<bool> {
 protected:
  NetworkServiceTaskSchedulerTest() {
    if (IsPerPriorityQueuesEnabled()) {
      feature_list_.InitAndEnableFeature(
          features::kNetworkServicePerPriorityTaskQueues);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kNetworkServicePerPriorityTaskQueues);
    }
  }

  NetworkServiceTaskScheduler* scheduler() {
    return task_environment_.scheduler();
  }

  bool IsPerPriorityQueuesEnabled() const { return GetParam(); }

 private:
  NetworkServiceTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that tasks posted to the default task runner provided by the scheduler
// are executed in order. Also verifies that the scheduler sets the current
// thread's default task runner.
TEST_P(NetworkServiceTaskSchedulerTest, DefaultQueuePostsTasksInOrder) {
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
TEST_P(NetworkServiceTaskSchedulerTest,
       MultipleQueuesHighPriorityTaskRunsFirst) {
  scoped_refptr<base::SingleThreadTaskRunner> tq1 =
      scheduler()->GetTaskRunner(net::RequestPriority::DEFAULT_PRIORITY);
  scoped_refptr<base::SingleThreadTaskRunner> tq2 =
      scheduler()->GetTaskRunner(net::RequestPriority::HIGHEST);

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
TEST_P(NetworkServiceTaskSchedulerTest, PostToAllPriorites) {
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
      ->GetTaskRunner(net::RequestPriority::THROTTLED)
      ->PostTask(FROM_HERE, task_1.Get());
  scheduler()
      ->GetTaskRunner(net::RequestPriority::IDLE)
      ->PostTask(FROM_HERE, task_2.Get());
  scheduler()
      ->GetTaskRunner(net::RequestPriority::LOWEST)
      ->PostTask(FROM_HERE, task_3.Get());
  scheduler()
      ->GetTaskRunner(net::RequestPriority::LOW)
      ->PostTask(FROM_HERE, task_4.Get());
  scheduler()
      ->GetTaskRunner(net::RequestPriority::MEDIUM)
      ->PostTask(FROM_HERE, task_5.Get());
  scheduler()
      ->GetTaskRunner(net::RequestPriority::HIGHEST)
      ->PostTask(FROM_HERE, task_6.Get());

  scheduler()
      ->GetTaskRunner(net::RequestPriority::THROTTLED)
      ->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Tests the `SetUpNetTaskRunners`.
// Verifies that after calling `SetupNetTaskRunners`, `net::GetTaskRunner`
// returns the appropriate task runners managed by the scheduler for all
// priority levels.
TEST_P(NetworkServiceTaskSchedulerTest,
       SetUpNetTaskRunnersIntegratesWithNetGetTaskRunner) {
  scheduler()->SetUpNetTaskRunnersForTesting();

  if (IsPerPriorityQueuesEnabled()) {
    for (int i = 0; i < net::NUM_PRIORITIES; ++i) {
      net::RequestPriority priority = static_cast<net::RequestPriority>(i);
      EXPECT_EQ(net::GetTaskRunner(priority).get(),
                scheduler()->GetTaskRunner(priority).get());
    }
  } else {
    // When per-priority queues are disabled, all priorities except HIGHEST
    // should map to the default task queue.
    EXPECT_NE(scheduler()->GetTaskRunner(net::RequestPriority::HIGHEST).get(),
              scheduler()->GetDefaultTaskRunner().get());
    for (int i = 0; i < net::NUM_PRIORITIES; ++i) {
      net::RequestPriority priority = static_cast<net::RequestPriority>(i);
      if (priority == net::RequestPriority::HIGHEST) {
        EXPECT_EQ(net::GetTaskRunner(priority).get(),
                  scheduler()->GetTaskRunner(priority).get());
      } else {
        EXPECT_EQ(net::GetTaskRunner(priority).get(),
                  scheduler()->GetDefaultTaskRunner().get());
      }
    }
  }

  // Also, verify that the default task runner is set up correctly.
  EXPECT_EQ(scheduler()->GetDefaultTaskRunner().get(),
            base::SingleThreadTaskRunner::GetCurrentDefault().get());
}

INSTANTIATE_TEST_SUITE_P(All, NetworkServiceTaskSchedulerTest, testing::Bool());

}  // namespace
}  // namespace network
