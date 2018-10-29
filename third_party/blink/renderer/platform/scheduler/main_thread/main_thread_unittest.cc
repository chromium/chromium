// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread.h"

#include <stddef.h>
#include <memory>

#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/testing/scoped_scheduler_overrider.h"

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace main_thread_unittest {

const int kWorkBatchSize = 2;

using ::testing::_;

class MockTask {
 public:
  MOCK_METHOD0(Run, void());
};

class MockTaskObserver : public Thread::TaskObserver {
 public:
  MOCK_METHOD1(WillProcessTask, void(const base::PendingTask&));
  MOCK_METHOD1(DidProcessTask, void(const base::PendingTask&));
};

class MainThreadTest : public testing::Test {
 public:
  MainThreadTest() = default;

  void SetUp() override {
    clock_.Advance(base::TimeDelta::FromMicroseconds(5000));
    scheduler_.reset(new MainThreadSchedulerImpl(
        base::sequence_manager::SequenceManagerForTest::Create(
            &message_loop_, message_loop_.task_runner(), &clock_),
        base::nullopt));
    scheduler_overrider_ =
        std::make_unique<ScopedSchedulerOverrider>(scheduler_.get());
    thread_ = Thread::Current();
  }

  ~MainThreadTest() override = default;

  void SetWorkBatchSizeForTesting(int work_batch_size) {
    scheduler_->GetSchedulerHelperForTesting()->SetWorkBatchSizeForTesting(
        work_batch_size);
  }

  void TearDown() override { scheduler_->Shutdown(); }

 protected:
  base::MessageLoop message_loop_;
  base::SimpleTestTickClock clock_;
  std::unique_ptr<MainThreadSchedulerImpl> scheduler_;
  std::unique_ptr<ScopedSchedulerOverrider> scheduler_overrider_;
  Thread* thread_;

  DISALLOW_COPY_AND_ASSIGN(MainThreadTest);
};

TEST_F(MainThreadTest, TestTaskObserver) {
  MockTaskObserver observer;
  thread_->AddTaskObserver(&observer);
  MockTask task;

  {
    testing::InSequence sequence;
    EXPECT_CALL(observer, WillProcessTask(_));
    EXPECT_CALL(task, Run());
    EXPECT_CALL(observer, DidProcessTask(_));
  }

  message_loop_.task_runner()->PostTask(
      FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task)));
  base::RunLoop().RunUntilIdle();
  thread_->RemoveTaskObserver(&observer);
}

TEST_F(MainThreadTest, TestWorkBatchWithOneTask) {
  MockTaskObserver observer;
  thread_->AddTaskObserver(&observer);
  MockTask task;

  SetWorkBatchSizeForTesting(kWorkBatchSize);
  {
    testing::InSequence sequence;
    EXPECT_CALL(observer, WillProcessTask(_));
    EXPECT_CALL(task, Run());
    EXPECT_CALL(observer, DidProcessTask(_));
  }

  message_loop_.task_runner()->PostTask(
      FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task)));
  base::RunLoop().RunUntilIdle();
  thread_->RemoveTaskObserver(&observer);
}

TEST_F(MainThreadTest, TestWorkBatchWithTwoTasks) {
  MockTaskObserver observer;
  thread_->AddTaskObserver(&observer);
  MockTask task1;
  MockTask task2;

  SetWorkBatchSizeForTesting(kWorkBatchSize);
  {
    testing::InSequence sequence;
    EXPECT_CALL(observer, WillProcessTask(_));
    EXPECT_CALL(task1, Run());
    EXPECT_CALL(observer, DidProcessTask(_));

    EXPECT_CALL(observer, WillProcessTask(_));
    EXPECT_CALL(task2, Run());
    EXPECT_CALL(observer, DidProcessTask(_));
  }

  message_loop_.task_runner()->PostTask(
      FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task1)));
  message_loop_.task_runner()->PostTask(
      FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task2)));
  base::RunLoop().RunUntilIdle();
  thread_->RemoveTaskObserver(&observer);
}

TEST_F(MainThreadTest, TestWorkBatchWithThreeTasks) {
  MockTaskObserver observer;
  thread_->AddTaskObserver(&observer);
  MockTask task1;
  MockTask task2;
  MockTask task3;

  SetWorkBatchSizeForTesting(kWorkBatchSize);
  {
    testing::InSequence sequence;
    EXPECT_CALL(observer, WillProcessTask(_));
    EXPECT_CALL(task1, Run());
    EXPECT_CALL(observer, DidProcessTask(_));

    EXPECT_CALL(observer, WillProcessTask(_));
    EXPECT_CALL(task2, Run());
    EXPECT_CALL(observer, DidProcessTask(_));

    EXPECT_CALL(observer, WillProcessTask(_));
    EXPECT_CALL(task3, Run());
    EXPECT_CALL(observer, DidProcessTask(_));
  }

  message_loop_.task_runner()->PostTask(
      FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task1)));
  message_loop_.task_runner()->PostTask(
      FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task2)));
  message_loop_.task_runner()->PostTask(
      FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task3)));
  base::RunLoop().RunUntilIdle();
  thread_->RemoveTaskObserver(&observer);
}

void EnterRunLoop(base::MessageLoop* message_loop, Thread* thread) {
  // Note: blink::Threads do not support nested run loops, which is why we use a
  // run loop directly.
  base::RunLoop run_loop;
  message_loop->task_runner()->PostTask(
      FROM_HERE, WTF::Bind(&base::RunLoop::Quit, WTF::Unretained(&run_loop)));
  message_loop->SetNestableTasksAllowed(true);
  run_loop.Run();
}

TEST_F(MainThreadTest, TestNestedRunLoop) {
  MockTaskObserver observer;
  thread_->AddTaskObserver(&observer);

  {
    testing::InSequence sequence;

    // One callback for EnterRunLoop.
    EXPECT_CALL(observer, WillProcessTask(_));

    // A pair for ExitRunLoopTask.
    EXPECT_CALL(observer, WillProcessTask(_));
    EXPECT_CALL(observer, DidProcessTask(_));

    // A final callback for EnterRunLoop.
    EXPECT_CALL(observer, DidProcessTask(_));
  }

  message_loop_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&EnterRunLoop, base::Unretained(&message_loop_),
                                base::Unretained(thread_)));
  base::RunLoop().RunUntilIdle();
  thread_->RemoveTaskObserver(&observer);
}

}  // namespace main_thread_unittest
}  // namespace scheduler
}  // namespace blink
