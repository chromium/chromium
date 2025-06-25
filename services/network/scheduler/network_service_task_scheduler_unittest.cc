// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/scheduler/network_service_task_scheduler.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "net/base/task/task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

using QueueType = NetworkServiceTaskQueues::QueueType;

using StrictMockTask =
    testing::StrictMock<base::MockCallback<base::RepeatingCallback<void()>>>;

class NetworkServiceTaskSchedulerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment;
};

// Tests that tasks posted to the default task runner provided by the scheduler
// are executed in order. Also verifies that the scheduler sets the current
// thread's default task runner.
TEST_F(NetworkServiceTaskSchedulerTest, DefaultQueuePostsTasksInOrder) {
  auto scheduler = NetworkServiceTaskScheduler::CreateForTesting();

  scoped_refptr<base::SingleThreadTaskRunner> tq =
      scheduler->GetTaskRunner(QueueType::kDefault);
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

// Tests that tasks posted to different priority queues (default and high
// priority) are executed according to their priority, with high priority tasks
// running before default priority tasks.
TEST_F(NetworkServiceTaskSchedulerTest,
       MultipleQueuesHighPriorityTaskRunsFirst) {
  auto scheduler = NetworkServiceTaskScheduler::CreateForTesting();

  scoped_refptr<base::SingleThreadTaskRunner> tq1 =
      scheduler->GetTaskRunner(QueueType::kDefault);
  scoped_refptr<base::SingleThreadTaskRunner> tq2 =
      scheduler->GetTaskRunner(QueueType::kHighPriority);

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

// Tests that tasks posted to the current thread's default task runner *before*
// the NetworkServiceTaskScheduler is instantiated are still executed
// correctly once the scheduler is up and running.
TEST_F(NetworkServiceTaskSchedulerTest,
       PendingTasksRunAfterSchedulerInitialization) {
  StrictMockTask task_1;
  StrictMockTask task_2;

  testing::InSequence s;
  EXPECT_CALL(task_1, Run);
  EXPECT_CALL(task_2, Run);

  base::RunLoop run_loop;

  // Post a task before NetworkServiceTaskScheduler is instantiated.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              task_1.Get());
  auto scheduler = NetworkServiceTaskScheduler::CreateForTesting();
  scoped_refptr<base::SingleThreadTaskRunner> tq =
      scheduler->GetTaskRunner(QueueType::kDefault);
  tq->PostTask(FROM_HERE, task_2.Get());
  tq->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Tests a combination of pending tasks (posted before scheduler instantiation)
// and tasks posted to different priority queues after the scheduler is
// instantiated. Verifies that all tasks run in the correct order based on
// posting time and priority.
TEST_F(NetworkServiceTaskSchedulerTest,
       PendingAndPostedTasksRunInCorrectOrder) {
  StrictMockTask task_1;
  StrictMockTask task_2;
  StrictMockTask task_3;
  StrictMockTask task_4;

  testing::InSequence s;
  // Prior tasks run at first.
  EXPECT_CALL(task_1, Run);
  EXPECT_CALL(task_2, Run);
  // High priority task runs.
  EXPECT_CALL(task_4, Run);
  // Default priority task runs after a high priority task runs.
  EXPECT_CALL(task_3, Run);

  base::RunLoop run_loop;

  // Post tasks before NetworkServiceTaskScheduler is instantiated.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              task_1.Get());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              task_2.Get());

  auto scheduler = NetworkServiceTaskScheduler::CreateForTesting();
  scoped_refptr<base::SingleThreadTaskRunner> tq_default =
      scheduler->GetTaskRunner(QueueType::kDefault);
  scoped_refptr<base::SingleThreadTaskRunner> tq_high =
      scheduler->GetTaskRunner(QueueType::kHighPriority);
  tq_default->PostTask(FROM_HERE, task_3.Get());
  tq_high->PostTask(FROM_HERE, task_4.Get());
  tq_default->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Tests the `SetUpNetTaskRunners`.
// Verifies that after calling `SetupNetTaskRunners`, `net::GetTaskRunner`
// returns the appropriate task runners managed by the scheduler.
TEST_F(NetworkServiceTaskSchedulerTest,
       SetUpNetTaskRunnersIntegratesWithNetGetTaskRunner) {
  auto scheduler = NetworkServiceTaskScheduler::CreateForTesting();
  scheduler->SetUpNetTaskRunnersForTesting();

  auto scheduler_high_priority_runner =
      scheduler->GetTaskRunner(QueueType::kHighPriority);
  auto net_highest_priority_runner = net::GetTaskRunner(net::HIGHEST);

  EXPECT_EQ(net_highest_priority_runner.get(),
            scheduler_high_priority_runner.get());

  auto scheduler_default_runner = scheduler->GetTaskRunner(QueueType::kDefault);
  auto net_medium_priority_runner = net::GetTaskRunner(net::MEDIUM);
  EXPECT_EQ(net_medium_priority_runner.get(), scheduler_default_runner.get());
  EXPECT_EQ(net_medium_priority_runner.get(),
            base::SingleThreadTaskRunner::GetCurrentDefault().get());
}

}  // namespace
}  // namespace network
