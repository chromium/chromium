// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_task_executor.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "content/browser/scheduler/browser_io_thread_delegate.h"
#include "content/browser/scheduler/browser_task_priority.h"
#include "content/browser/scheduler/browser_task_queues.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using ::base::TaskPriority;
using ::testing::ElementsAre;
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

TEST_F(BrowserTaskExecutorTest, RunAllPendingTasksForTestingOnUI) {
  StrictMockTask task_1;
  StrictMockTask task_2;
  EXPECT_CALL(task_1, Run).WillOnce([&]() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                                task_2.Get());
  });

  GetUIThreadTaskRunner({})->PostTask(FROM_HERE, task_1.Get());

  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::UI);

  // Cleanup pending tasks, as BrowserTaskEnvironment will run them.
  Mock::VerifyAndClearExpectations(&task_1);
  EXPECT_CALL(task_2, Run);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
}

TEST_F(BrowserTaskExecutorTest, RunAllPendingTasksForTestingOnIO) {
  StrictMockTask task_1;
  StrictMockTask task_2;
  EXPECT_CALL(task_1, Run).WillOnce([&]() {
    GetIOThreadTaskRunner({})->PostTask(FROM_HERE, task_2.Get());
  });

  GetIOThreadTaskRunner({})->PostTask(FROM_HERE, task_1.Get());

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

  EXPECT_CALL(task_1, Run).WillOnce([&]() {
    GetIOThreadTaskRunner({})->PostTask(FROM_HERE, task_2.Get());
    BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(
        BrowserThread::IO);
  });
  EXPECT_CALL(task_2, Run).WillOnce([&]() {
    GetIOThreadTaskRunner({})->PostTask(FROM_HERE, task_3.Get());
  });

  GetIOThreadTaskRunner({})->PostTask(FROM_HERE, task_1.Get());
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);

  // Cleanup pending tasks, as BrowserTaskEnvironment will run them.
  Mock::VerifyAndClearExpectations(&task_1);
  Mock::VerifyAndClearExpectations(&task_2);
  EXPECT_CALL(task_3, Run);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
}

TEST_F(BrowserTaskExecutorTest, GetTaskRunnerWithBrowserTaskTraits) {
  StrictMockTask task_1;

  GetUIThreadTaskRunner({BrowserTaskType::kUserInput})
      ->PostTask(FROM_HERE, task_1.Get());

  EXPECT_CALL(task_1, Run);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::UI);
}

TEST_F(BrowserTaskExecutorTest, BrowserTaskTraitsMapToProperPriorities) {
  EXPECT_EQ(BrowserTaskExecutor::GetQueueType({TaskPriority::BEST_EFFORT}),
            QueueType::kBestEffort);
  EXPECT_EQ(BrowserTaskExecutor::GetQueueType({TaskPriority::USER_VISIBLE}),
            QueueType::kUserVisible);
  EXPECT_EQ(BrowserTaskExecutor::GetQueueType({TaskPriority::USER_BLOCKING}),
            QueueType::kUserBlocking);

  EXPECT_EQ(BrowserTaskExecutor::GetQueueType({BrowserTaskType::kDefault}),
            QueueType::kUserBlocking);
  EXPECT_EQ(BrowserTaskExecutor::GetQueueType(
                {BrowserTaskType::kServiceWorkerStorageControlResponse}),
            QueueType::kServiceWorkerStorageControlResponse);

  EXPECT_EQ(BrowserTaskExecutor::GetQueueType({}), QueueType::kUserBlocking);
  EXPECT_EQ(BrowserTaskExecutor::GetQueueType({BrowserTaskType::kStartup}),
            QueueType::kStartup);
}

TEST_F(BrowserTaskExecutorTest, UIThreadTaskRunnerHasSamePriorityAsUIBlocking) {
  auto ui_blocking = GetUIThreadTaskRunner({TaskPriority::USER_BLOCKING});
  auto thread_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();

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
              internal::CreateBrowserTaskPrioritySettings(),
              SubclassCreatesDefaultTaskRunner{},
              base::test::TaskEnvironment::MainThreadType::UI,
              base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
      std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler =
          BrowserUIThreadScheduler::CreateForTesting(sequence_manager());

      DeferredInitFromSubclass(
          browser_ui_thread_scheduler->GetHandle()->GetBrowserTaskRunner(
              QueueType::kDefault));
      BrowserTaskExecutor::CreateForTesting(
          std::move(browser_ui_thread_scheduler),
          BrowserIOThreadDelegate::CreateForTesting(sequence_manager()));
    }
  };

 public:
  ~BrowserTaskExecutorWithCustomSchedulerTest() override {
    BrowserTaskExecutor::ResetForTesting();
  }

 protected:
  TaskEnvironmentWithCustomScheduler task_environment_;
};

TEST_F(BrowserTaskExecutorWithCustomSchedulerTest,
       AllTasksExceptBestEffortRunDuringStartup) {
  StrictMockTask best_effort;
  StrictMockTask user_visible;
  StrictMockTask user_blocking;
  StrictMockTask startup;

  GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE, best_effort.Get());
  GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
      ->PostTask(FROM_HERE, user_visible.Get());
  GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
      ->PostTask(FROM_HERE, user_blocking.Get());
  GetUIThreadTaskRunner({BrowserTaskType::kStartup})
      ->PostTask(FROM_HERE, startup.Get());

  EXPECT_CALL(user_visible, Run);
  EXPECT_CALL(user_blocking, Run);
  EXPECT_CALL(startup, Run);

  task_environment_.RunUntilIdle();
}

TEST_F(BrowserTaskExecutorWithCustomSchedulerTest,
       BestEffortTasksRunAfterStartup) {
  auto ui_best_effort_runner =
      GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT});

  StrictMockTask best_effort;

  ui_best_effort_runner->PostTask(FROM_HERE, best_effort.Get());
  ui_best_effort_runner->PostDelayedTask(FROM_HERE, best_effort.Get(),
                                         base::Milliseconds(100));
  GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostDelayedTask(FROM_HERE, best_effort.Get(), base::Milliseconds(100));
  GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE, best_effort.Get());
  task_environment_.RunUntilIdle();

  BrowserTaskExecutor::OnStartupComplete();
  EXPECT_CALL(best_effort, Run).Times(4);
  task_environment_.FastForwardBy(base::Milliseconds(100));
}

TEST_F(BrowserTaskExecutorWithCustomSchedulerTest,
       OnlyDefaultTaskRunnerActiveAfterCreationForIO) {
  StrictMockTask task;
  for (size_t i = 0; i < BrowserTaskQueues::kNumQueueTypes; ++i) {
    auto queue_type = static_cast<QueueType>(i);
    if (queue_type != QueueType::kDefault) {
      BrowserTaskExecutor::Get()
          ->browser_io_thread_handle_->GetBrowserTaskRunner(queue_type)
          ->PostTask(FROM_HERE, task.Get());
    }
  }

  StrictMockTask default_task;
  BrowserTaskExecutor::Get()
      ->browser_io_thread_handle_->GetDefaultTaskRunner()
      ->PostTask(FROM_HERE, default_task.Get());
  EXPECT_CALL(default_task, Run);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
}

class BrowserTaskExecutorWithCustomClientTest : public testing::Test {
 private:
  // We can't use the BrowserTaskEnvironment because it has a lot of logic
  // which end up enabling the queues.
  class CustomTaskEnvironment : public base::test::TaskEnvironment {
   public:
    CustomTaskEnvironment()
        : base::test::TaskEnvironment(
              internal::CreateBrowserTaskPrioritySettings(),
              SubclassCreatesDefaultTaskRunner{},
              base::test::TaskEnvironment::MainThreadType::UI,
              base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
      std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler =
          BrowserUIThreadScheduler::CreateForTesting(sequence_manager());
      DeferredInitFromSubclass(
          browser_ui_thread_scheduler->GetHandle()->GetBrowserTaskRunner(
              QueueType::kDefault));
      BrowserTaskExecutor::CreateForTesting(
          std::move(browser_ui_thread_scheduler),
          BrowserIOThreadDelegate::CreateForTesting(sequence_manager()));
    }
  };

  class TaskRunnerNotEnabledContentBrowserClient : public ContentBrowserClient {
   public:
    TaskRunnerNotEnabledContentBrowserClient() {
      old_browser_client_ = SetBrowserClientForTesting(this);
    }
    ~TaskRunnerNotEnabledContentBrowserClient() override {
      EXPECT_EQ(this, SetBrowserClientForTesting(old_browser_client_));
    }
    void RunStartupCompleteCallback() { std::move(callback_).Run(); }

   private:
    raw_ptr<ContentBrowserClient> old_browser_client_ = nullptr;
    base::OnceClosure callback_;
    void OnUiTaskRunnerReady(
        base::OnceClosure enable_native_ui_task_execution_callback) override {
      callback_ = std::move(enable_native_ui_task_execution_callback);
    }
  };

 public:
  ~BrowserTaskExecutorWithCustomClientTest() override {
    BrowserTaskExecutor::ResetForTesting();
  }

 protected:
  TaskRunnerNotEnabledContentBrowserClient content_browser_client_;
  CustomTaskEnvironment task_environment_;
};

TEST_F(BrowserTaskExecutorWithCustomClientTest,
       TasksNotRunIfContentClientDoesNotRunCallback) {
  StrictMockTask task;

  GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
      ->PostTask(FROM_HERE, task.Get());
  GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
      ->PostTask(FROM_HERE, task.Get());

  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::UI);

  content_browser_client_.RunStartupCompleteCallback();

  EXPECT_CALL(task, Run).Times(2);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::UI);
}

class BrowserTaskExecutorWithNoContentClientTest : public testing::Test {
 public:
  BrowserTaskExecutorWithNoContentClientTest() {
    old_client_ = GetContentClientForTesting();
    SetContentClient(nullptr);
    task_environment_ = std::make_unique<BrowserTaskEnvironment>();
  }
  ~BrowserTaskExecutorWithNoContentClientTest() override {
    SetContentClient(old_client_);
    BrowserTaskExecutor::ResetForTesting();
  }

 private:
  raw_ptr<ContentClient> old_client_ = nullptr;
  std::unique_ptr<BrowserTaskEnvironment> task_environment_ = nullptr;
};

TEST_F(BrowserTaskExecutorWithNoContentClientTest,
       TasksRunIfContentClientNotSet) {
  StrictMockTask task;
  GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
      ->PostTask(FROM_HERE, task.Get());
  GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
      ->PostTask(FROM_HERE, task.Get());

  EXPECT_CALL(task, Run).Times(2);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::UI);
}

}  // namespace content
