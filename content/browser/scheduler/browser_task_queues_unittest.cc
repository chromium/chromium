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
using ::testing::Invoke;
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

TEST_F(BrowserTaskQueuesTest, NoTaskRunsUntilQueuesAreEnabled) {
  StrictMockTask task;
  for (size_t i = 0; i < BrowserTaskQueues::kNumQueueTypes; ++i) {
    handle_->GetBrowserTaskRunner(static_cast<QueueType>(i))
        ->PostTask(FROM_HERE, task.Get());
  }

  {
    RunLoop run_loop;
    handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
    // Default queue isn't disabled and should run.
    EXPECT_CALL(task, Run).Times(1);
    run_loop.Run();
  }

  handle_->OnStartupComplete();

  {
    RunLoop run_loop;
    handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
    // All tasks should run except default queue which already run as
    // it's not disabled during startup.
    EXPECT_CALL(task, Run).Times(BrowserTaskQueues::kNumQueueTypes - 1);
    run_loop.Run();
  }
}

TEST_F(BrowserTaskQueuesTest, OnlyDefaultQueueRunsTasksOnCreation) {
  StrictMockTask task;
  for (size_t i = 0; i < BrowserTaskQueues::kNumQueueTypes; ++i) {
    if (static_cast<QueueType>(i) != QueueType::kDefault) {
      handle_->GetBrowserTaskRunner(static_cast<QueueType>(i))
          ->PostTask(FROM_HERE, task.Get());
    }
  }

  StrictMockTask default_task;
  handle_->GetDefaultTaskRunner()->PostTask(FROM_HERE, default_task.Get());

  {
    RunLoop run_loop;
    handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
    EXPECT_CALL(default_task, Run);
    run_loop.Run();
  }
}

TEST_F(BrowserTaskQueuesTest, TasksRunWhenQueuesAreEnabled) {
  StrictMockTask task;
  for (size_t i = 0; i < BrowserTaskQueues::kNumQueueTypes; ++i) {
    handle_->GetBrowserTaskRunner(static_cast<QueueType>(i))
        ->PostTask(FROM_HERE, task.Get());
  }

  {
    RunLoop run_loop;
    handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
    // Default queue isn't disabled.
    EXPECT_CALL(task, Run).Times(1);
    run_loop.Run();
  }

  handle_->OnStartupComplete();

  {
    RunLoop run_loop;
    // All tasks should run, except default queue which is already run
    // as default queue isn't disabled.
    handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
    EXPECT_CALL(task, Run).Times(BrowserTaskQueues::kNumQueueTypes - 1);
    run_loop.Run();
  }
}

TEST_F(BrowserTaskQueuesTest, SimplePosting) {
  handle_->OnStartupComplete();
  scoped_refptr<base::SingleThreadTaskRunner> tq =
      handle_->GetBrowserTaskRunner(QueueType::kUserBlocking);

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
  EXPECT_CALL(task, Run).WillOnce(Invoke([&]() {
    for (size_t i = 0; i < BrowserTaskQueues::kNumQueueTypes; ++i) {
      handle_->GetBrowserTaskRunner(static_cast<QueueType>(i))
          ->PostTask(FROM_HERE, followup_task.Get());
    }
  }));

  handle_->GetBrowserTaskRunner(QueueType::kUserBlocking)
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

  EXPECT_CALL(task_1, Run).WillOnce(Invoke([&]() {
    handle_->GetBrowserTaskRunner(QueueType::kUserBlocking)
        ->PostTask(FROM_HERE, task_2.Get());
    RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);
    handle_->ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }));

  EXPECT_CALL(task_2, Run).WillOnce(Invoke([&]() {
    handle_->GetBrowserTaskRunner(QueueType::kUserBlocking)
        ->PostTask(FROM_HERE, task_3.Get());
  }));

  handle_->GetBrowserTaskRunner(QueueType::kUserBlocking)
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
  handle_->GetBrowserTaskRunner(QueueType::kUserBlocking)
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

  EXPECT_CALL(task_1, Run).WillOnce(Invoke([&]() {
    // This task should not run as it is posted after the
    // RunAllPendingTasksForTesting() call
    handle_->GetBrowserTaskRunner(QueueType::kBestEffort)
        ->PostTask(FROM_HERE, task_3.Get());
    handle_->OnStartupComplete();
  }));
  EXPECT_CALL(task_2, Run);

  handle_->GetBrowserTaskRunner(QueueType::kUserBlocking)
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

}  // namespace
}  // namespace content
