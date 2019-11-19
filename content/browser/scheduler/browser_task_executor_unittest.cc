// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_task_executor.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/scheduler/browser_io_thread_delegate.h"
#include "content/browser/scheduler/browser_task_queues.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using ::base::TaskPriority;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::SizeIs;

using QueueType = BrowserTaskQueues::QueueType;

class BrowserTaskExecutorTest : public testing::Test {
 private:
  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

using StrictMockTask =
    testing::StrictMock<base::MockCallback<base::RepeatingCallback<void()>>>;

TEST_F(BrowserTaskExecutorTest, RegisterExecutorForBothThreads) {
  base::PostTask(FROM_HERE, {BrowserThread::UI}, base::BindOnce([]() {
                   EXPECT_THAT(base::GetTaskExecutorForCurrentThread(),
                               NotNull());
                 }));

  base::PostTask(FROM_HERE, {BrowserThread::IO}, base::BindOnce([]() {
                   EXPECT_THAT(base::GetTaskExecutorForCurrentThread(),
                               NotNull());
                 }));

  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::UI);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
}

TEST_F(BrowserTaskExecutorTest, RunAllPendingTasksForTestingOnUI) {
  StrictMockTask task_1;
  StrictMockTask task_2;
  EXPECT_CALL(task_1, Run).WillOnce(testing::Invoke([&]() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task_2.Get());
  }));

  base::PostTask(FROM_HERE, {BrowserThread::UI}, task_1.Get());

  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::UI);

  // Cleanup pending tasks, as BrowserTaskEnvironment will run them.
  Mock::VerifyAndClearExpectations(&task_1);
  EXPECT_CALL(task_2, Run);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
}

TEST_F(BrowserTaskExecutorTest, RunAllPendingTasksForTestingOnIO) {
  StrictMockTask task_1;
  StrictMockTask task_2;
  EXPECT_CALL(task_1, Run).WillOnce(testing::Invoke([&]() {
    base::PostTask(FROM_HERE, {BrowserThread::IO}, task_2.Get());
  }));

  base::PostTask(FROM_HERE, {BrowserThread::IO}, task_1.Get());

  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);

  // Cleanup pending tasks, as BrowserTaskEnvironment will run them.
  Mock::VerifyAndClearExpectations(&task_1);
  EXPECT_CALL(task_2, Run);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
}

TEST_F(BrowserTaskExecutorTest, RunAllPendingTasksForTestingOnIOIsReentrant) {
  StrictMockTask task_1;
  StrictMockTask task_2;
  StrictMockTask task_3;

  EXPECT_CALL(task_1, Run).WillOnce(Invoke([&]() {
    base::PostTask(FROM_HERE, {BrowserThread::IO}, task_2.Get());
    BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(
        BrowserThread::IO);
  }));
  EXPECT_CALL(task_2, Run).WillOnce(Invoke([&]() {
    base::PostTask(FROM_HERE, {BrowserThread::IO}, task_3.Get());
  }));

  base::PostTask(FROM_HERE, {BrowserThread::IO}, task_1.Get());
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);

  // Cleanup pending tasks, as BrowserTaskEnvironment will run them.
  Mock::VerifyAndClearExpectations(&task_1);
  Mock::VerifyAndClearExpectations(&task_2);
  EXPECT_CALL(task_3, Run);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
}

// Helper to perform the same tets for all BrowserThread::ID values.
class BrowserTaskTraitsMappingTest : public BrowserTaskExecutorTest {
 protected:
  class TestExecutor : public BaseBrowserTaskExecutor {
   public:
    ~TestExecutor() override = default;

    BrowserThread::ID GetCurrentThreadID() const override {
      NOTREACHED();
      return BrowserThread::UI;
    }

    const scoped_refptr<base::SequencedTaskRunner>& GetContinuationTaskRunner()
        override {
      return dummy_;
    }

   private:
    scoped_refptr<base::SequencedTaskRunner> dummy_;
  };

  template <BrowserThread::ID ID>
  void CheckExpectations() {
    EXPECT_EQ(GetQueueType({ID, TaskPriority::BEST_EFFORT}),
              QueueType::kBestEffort);
    EXPECT_EQ(GetQueueType({ID, TaskPriority::USER_VISIBLE}),
              QueueType::kUserVisible);
    EXPECT_EQ(GetQueueType({ID, TaskPriority::USER_BLOCKING}),
              QueueType::kUserBlocking);

    EXPECT_EQ(GetQueueType({ID, BrowserTaskType::kBootstrap}),
              QueueType::kBootstrap);
    EXPECT_EQ(GetQueueType({ID, BrowserTaskType::kDefault}),
              QueueType::kUserBlocking);
    EXPECT_EQ(GetQueueType({ID, BrowserTaskType::kNavigation}),
              QueueType::kNavigationAndPreconnection);
    EXPECT_EQ(GetQueueType({ID, BrowserTaskType::kPreconnect}),
              QueueType::kNavigationAndPreconnection);

    EXPECT_EQ(GetQueueType({ID}), QueueType::kUserBlocking);
  }

 private:
  QueueType GetQueueType(const base::TaskTraits& traits) {
    return test_executor_.GetThreadIdAndQueueType(traits).queue_type;
  }

  TestExecutor test_executor_;
};

TEST_F(BrowserTaskTraitsMappingTest, BrowserTaskTraitsMapToProperPriorities) {
  CheckExpectations<BrowserThread::UI>();
  CheckExpectations<BrowserThread::IO>();
}

TEST_F(BrowserTaskTraitsMappingTest,
       UIThreadTaskRunnerHasSamePriorityAsUIBlocking) {
  auto ui_blocking = base::CreateSingleThreadTaskRunner(
      {BrowserThread::UI, TaskPriority::USER_BLOCKING});
  auto thread_task_runner = base::ThreadTaskRunnerHandle::Get();

  std::vector<int> order;
  ui_blocking->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { order.push_back(1); }));
  thread_task_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { order.push_back(10); }));
  ui_blocking->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { order.push_back(2); }));
  thread_task_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { order.push_back(20); }));

  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(order, ElementsAre(1, 10, 2, 20));
}

class BrowserTaskExecutorWithCustomSchedulerTest : public testing::Test {
 private:
  class TaskEnvironmentWithCustomScheduler
      : public base::test::TaskEnvironment {
   public:
    TaskEnvironmentWithCustomScheduler()
        : base::test::TaskEnvironment(
              SubclassCreatesDefaultTaskRunner{},
              base::test::TaskEnvironment::MainThreadType::UI,
              base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
      std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler =
          BrowserUIThreadScheduler::CreateForTesting(sequence_manager(),
                                                     GetTimeDomain());
      DeferredInitFromSubclass(
          browser_ui_thread_scheduler->GetHandle()->GetBrowserTaskRunner(
              QueueType::kDefault));
      BrowserTaskExecutor::CreateForTesting(
          std::move(browser_ui_thread_scheduler),
          BrowserIOThreadDelegate::CreateForTesting(sequence_manager()));
      BrowserTaskExecutor::BindToUIThreadForTesting();
    }
  };

 public:
  using QueueType = BrowserTaskQueues::QueueType;

  ~BrowserTaskExecutorWithCustomSchedulerTest() override {
    BrowserTaskExecutor::ResetForTesting();
  }

 protected:
  TaskEnvironmentWithCustomScheduler task_environment_;
};

TEST_F(BrowserTaskExecutorWithCustomSchedulerTest,
       UserVisibleOrBlockingTasksRunDuringStartup) {
  StrictMockTask best_effort;
  StrictMockTask user_visible;
  StrictMockTask user_blocking;

  base::PostTask(FROM_HERE,
                 {BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
                 best_effort.Get());
  base::PostTask(FROM_HERE,
                 {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
                 user_visible.Get());
  base::PostTask(FROM_HERE,
                 {BrowserThread::UI, base::TaskPriority::USER_BLOCKING},
                 user_blocking.Get());

  EXPECT_CALL(user_visible, Run);
  EXPECT_CALL(user_blocking, Run);

  task_environment_.RunUntilIdle();
}

TEST_F(BrowserTaskExecutorWithCustomSchedulerTest,
       BestEffortTasksRunAfterStartup) {
  auto ui_best_effort_runner = base::CreateSingleThreadTaskRunner(
      {BrowserThread::UI, base::TaskPriority::BEST_EFFORT});

  StrictMockTask best_effort;

  ui_best_effort_runner->PostTask(FROM_HERE, best_effort.Get());
  ui_best_effort_runner->PostDelayedTask(
      FROM_HERE, best_effort.Get(), base::TimeDelta::FromMilliseconds(100));
  base::PostDelayedTask(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
      best_effort.Get(), base::TimeDelta::FromMilliseconds(100));
  base::PostTask(FROM_HERE,
                 {BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
                 best_effort.Get());
  task_environment_.RunUntilIdle();

  BrowserTaskExecutor::EnableAllQueues();
  EXPECT_CALL(best_effort, Run).Times(4);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(100));
}

TEST_F(BrowserTaskExecutorTest, CurrentThread) {
  base::PostTask(
      FROM_HERE, {BrowserThread::UI}, base::BindOnce([]() {
        base::PostTask(
            FROM_HERE, {base::CurrentThread()}, base::BindOnce([]() {
              EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
            }));
      }));
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::UI);

  base::PostTask(
      FROM_HERE, {BrowserThread::IO}, base::BindOnce([]() {
        base::PostTask(
            FROM_HERE, {base::CurrentThread()}, base::BindOnce([]() {
              EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
            }));
      }));

  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
}

TEST_F(BrowserTaskExecutorTest, CurrentThreadAndOtherTraits) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));

  auto ui_task_runner = base::CreateSingleThreadTaskRunner({BrowserThread::UI});
  auto ui_best_effort_runner = base::CreateSingleThreadTaskRunner(
      {BrowserThread::UI, base::TaskPriority::BEST_EFFORT});
  auto ui_best_navigation_runner = base::CreateSingleThreadTaskRunner(
      {BrowserThread::UI, BrowserTaskType::kNavigation});

  EXPECT_EQ(ui_task_runner,
            base::CreateSingleThreadTaskRunner({base::CurrentThread()}));

  EXPECT_EQ(ui_best_effort_runner,
            base::CreateSingleThreadTaskRunner(
                {base::CurrentThread(), base::TaskPriority::BEST_EFFORT}));

  EXPECT_EQ(ui_best_navigation_runner,
            base::CreateSingleThreadTaskRunner(
                {base::CurrentThread(), BrowserTaskType::kNavigation}));
}

TEST_F(BrowserTaskExecutorTest, GetContinuationTaskRunner) {
  // Ensure task queue priorities are set.
  BrowserTaskExecutor::PostFeatureListSetup();
  std::vector<int> order;
  base::RunLoop run_loop;

  auto task1 = base::BindLambdaForTesting([&]() {
    order.push_back(1);
    run_loop.Quit();
  });
  auto task2 = base::BindLambdaForTesting([&]() { order.push_back(2); });
  auto task3 = base::BindLambdaForTesting([&]() { order.push_back(3); });

  base::PostTask(FROM_HERE, {BrowserThread::UI}, task1);

  // Post a bootstrap task whose continuation tasks should run before |task1|.
  base::PostTask(
      FROM_HERE, {BrowserThread::UI, BrowserTaskType::kBootstrap},
      base::BindLambdaForTesting([&]() {
        base::GetContinuationTaskRunner()->PostTask(FROM_HERE, task2);
        base::GetContinuationTaskRunner()->PostTask(FROM_HERE, task3);
      }));

  run_loop.Run();
  EXPECT_THAT(order, ElementsAre(2, 3, 1));
}

TEST_F(BrowserTaskExecutorTest, GetContinuationTaskRunnerWithNoTaskExecuting) {
  EXPECT_DCHECK_DEATH(base::GetContinuationTaskRunner());
}

}  // namespace content
