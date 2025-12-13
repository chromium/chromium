// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_task_queues.h"

#include <array>
#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "content/browser/scheduler/browser_task_priority.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using ::base::RunLoop;
using ::base::sequence_manager::CreateSequenceManagerOnCurrentThreadWithPump;
using ::base::sequence_manager::SequenceManager;
using ::testing::Mock;

using StrictMockTask =
    testing::StrictMock<base::MockCallback<base::RepeatingCallback<void()>>>;

using QueueType = BrowserTaskQueues::QueueType;

class BrowserTaskQueuesTest : public testing::Test {
 protected:
  BrowserTaskQueuesTest()
      : sequence_manager_(CreateSequenceManagerOnCurrentThreadWithPump(
            base::MessagePump::Create(base::MessagePumpType::DEFAULT),
            base::sequence_manager::SequenceManager::Settings::Builder()
                .SetPrioritySettings(
                    internal::CreateBrowserTaskPrioritySettings())
                .Build())),
        queues_(std::make_unique<BrowserTaskQueues>(BrowserThread::UI,
                                                    sequence_manager_.get())),
        handle_(queues_->GetHandle()) {
    sequence_manager_->SetDefaultTaskRunner(handle_->GetDefaultTaskRunner());
  }

  std::unique_ptr<SequenceManager> sequence_manager_;
  std::unique_ptr<BrowserTaskQueues> queues_;
  scoped_refptr<BrowserTaskQueues::Handle> handle_;
};

// Queues are disabled by default and only enabled by the BrowserTaskExecutor
// and so no task can be posted until the BrowserTaskExecutor is created. This
// allows an embedder to control when to enable the UI task queues. This state
// is required for WebView's async startup to work properly.
TEST_F(BrowserTaskQueuesTest, NoTaskRunsUntilQueuesAreEnabled) {
  StrictMockTask task;
  for (size_t i = 0; i < BrowserTaskQueues::kNumQueueTypes; ++i) {
    handle_->GetBrowserTaskRunner(static_cast<QueueType>(i))
        ->PostTask(FROM_HERE, task.Get());
  }

  {
    RunLoop run_loop;
    handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  handle_->OnStartupComplete();

  {
    RunLoop run_loop;
    handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
    EXPECT_CALL(task, Run).Times(BrowserTaskQueues::kNumQueueTypes);
    run_loop.Run();
  }
}

TEST_F(BrowserTaskQueuesTest, SimplePosting) {
  handle_->OnStartupComplete();
  scoped_refptr<base::SingleThreadTaskRunner> tq =
      handle_->GetBrowserTaskRunner(QueueType::kDefault);

  StrictMockTask task_1;
  StrictMockTask task_2;
  StrictMockTask task_3;

  {
    testing::InSequence s;
    EXPECT_CALL(task_1, Run);
    EXPECT_CALL(task_2, Run);
    EXPECT_CALL(task_3, Run);
  }

  tq->PostTask(FROM_HERE, task_1.Get());
  tq->PostTask(FROM_HERE, task_2.Get());
  tq->PostTask(FROM_HERE, task_3.Get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(BrowserTaskQueuesTest, RunAllPendingTasksForTesting) {
  handle_->OnStartupComplete();

  StrictMockTask task;
  StrictMockTask followup_task;
  EXPECT_CALL(task, Run).WillOnce([&]() {
    for (size_t i = 0; i < BrowserTaskQueues::kNumQueueTypes; ++i) {
      handle_->GetBrowserTaskRunner(static_cast<QueueType>(i))
          ->PostTask(FROM_HERE, followup_task.Get());
    }
  });

  handle_->GetBrowserTaskRunner(QueueType::kDefault)
      ->PostTask(FROM_HERE, task.Get());

  {
    RunLoop run_loop;
    handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  Mock::VerifyAndClearExpectations(&task);
  EXPECT_CALL(followup_task, Run).Times(BrowserTaskQueues::kNumQueueTypes);

  RunLoop run_loop;
  handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(BrowserTaskQueuesTest, RunAllPendingTasksForTestingRunsAllTasks) {
  constexpr size_t kTasksPerPriority = 100;
  handle_->OnStartupComplete();

  StrictMockTask task;
  EXPECT_CALL(task, Run).Times(BrowserTaskQueues::kNumQueueTypes *
                               kTasksPerPriority);
  for (size_t i = 0; i < BrowserTaskQueues::kNumQueueTypes; ++i) {
    for (size_t j = 0; j < kTasksPerPriority; ++j) {
      handle_->GetBrowserTaskRunner(static_cast<QueueType>(i))
          ->PostTask(FROM_HERE, task.Get());
    }
  }

  RunLoop run_loop;
  handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(BrowserTaskQueuesTest, RunAllPendingTasksForTestingIsReentrant) {
  handle_->OnStartupComplete();
  StrictMockTask task_1;
  StrictMockTask task_2;
  StrictMockTask task_3;

  EXPECT_CALL(task_1, Run).WillOnce([&]() {
    handle_->GetBrowserTaskRunner(QueueType::kDefault)
        ->PostTask(FROM_HERE, task_2.Get());
    RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);
    handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
    run_loop.Run();
  });

  EXPECT_CALL(task_2, Run).WillOnce([&]() {
    handle_->GetBrowserTaskRunner(QueueType::kDefault)
        ->PostTask(FROM_HERE, task_3.Get());
  });

  handle_->GetBrowserTaskRunner(QueueType::kDefault)
      ->PostTask(FROM_HERE, task_1.Get());

  RunLoop run_loop;
  handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(BrowserTaskQueuesTest,
       RunAllPendingTasksForTestingIgnoresBestEffortIfNotEnabled) {
  handle_->EnableAllExceptBestEffortQueues();
  StrictMockTask best_effort_task;
  StrictMockTask default_task;

  handle_->GetBrowserTaskRunner(QueueType::kBestEffort)
      ->PostTask(FROM_HERE, best_effort_task.Get());
  handle_->GetBrowserTaskRunner(QueueType::kDefault)
      ->PostTask(FROM_HERE, default_task.Get());

  EXPECT_CALL(default_task, Run);

  RunLoop run_loop;
  handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(BrowserTaskQueuesTest,
       RunAllPendingTasksForTestingRunsBestEffortTasksWhenEnabled) {
  handle_->EnableAllExceptBestEffortQueues();
  StrictMockTask task_1;
  StrictMockTask task_2;
  StrictMockTask task_3;

  EXPECT_CALL(task_1, Run).WillOnce([&]() {
    // This task should not run as it is posted after the
    // RunAllPendingTasksForTesting() call
    handle_->GetBrowserTaskRunner(QueueType::kBestEffort)
        ->PostTask(FROM_HERE, task_3.Get());
    handle_->OnStartupComplete();
  });
  EXPECT_CALL(task_2, Run);

  handle_->GetBrowserTaskRunner(QueueType::kDefault)
      ->PostTask(FROM_HERE, task_1.Get());
  handle_->GetBrowserTaskRunner(QueueType::kBestEffort)
      ->PostTask(FROM_HERE, task_2.Get());

  RunLoop run_loop;
  handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(BrowserTaskQueuesTest, HandleStillWorksWhenQueuesDestroyed) {
  handle_->OnStartupComplete();
  StrictMockTask task;
  queues_.reset();

  for (size_t i = 0; i < BrowserTaskQueues::kNumQueueTypes; ++i) {
    EXPECT_FALSE(handle_->GetBrowserTaskRunner(static_cast<QueueType>(i))
                     ->PostTask(FROM_HERE, base::DoNothing()));
  }

  RunLoop run_loop;
  handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(BrowserTaskQueuesTest, TaskIsRunWhenEnableQueueIsCalled) {
  StrictMockTask task;
  StrictMockTask user_visible;
  for (size_t i = 0; i < BrowserTaskQueues::kNumQueueTypes; ++i) {
    auto queue_type = static_cast<QueueType>(i);
    if (queue_type != QueueType::kUserVisible) {
      handle_->GetBrowserTaskRunner(queue_type)
          ->PostTask(FROM_HERE, task.Get());
    }
  }
  handle_->GetBrowserTaskRunner(QueueType::kUserVisible)
      ->PostTask(FROM_HERE, user_visible.Get());

  {
    RunLoop run_loop;
    handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  handle_->EnableTaskQueue(QueueType::kUserVisible);

  {
    RunLoop run_loop;
    handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
    EXPECT_CALL(user_visible, Run).Times(1);
    run_loop.Run();
  }
}

}  // namespace
}  // namespace content
