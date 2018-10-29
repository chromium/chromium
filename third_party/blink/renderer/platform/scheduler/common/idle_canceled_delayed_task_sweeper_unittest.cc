// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/idle_canceled_delayed_task_sweeper.h"

#include "base/task/sequence_manager/lazy_now.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/common/idle_helper.h"
#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_helper.h"

namespace blink {
namespace scheduler {

class TestClass {
 public:
  TestClass() : weak_factory_(this) {}

  void NopTask() {}

  base::WeakPtrFactory<TestClass> weak_factory_;
};

class IdleCanceledDelayedTaskSweeperTest : public testing::Test,
                                           public IdleHelper::Delegate {
 public:
  IdleCanceledDelayedTaskSweeperTest()
      : task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
            base::test::ScopedTaskEnvironment::ExecutionMode::QUEUED),
        scheduler_helper_(new MainThreadSchedulerHelper(
            base::sequence_manager::SequenceManagerForTest::Create(
                nullptr,
                task_environment_.GetMainThreadTaskRunner(),
                task_environment_.GetMockTickClock()),
            nullptr)),
        idle_helper_(
            new IdleHelper(scheduler_helper_.get(),
                           this,
                           "test",
                           base::TimeDelta::FromSeconds(30),
                           scheduler_helper_->NewTaskQueue(
                               MainThreadTaskQueue::QueueCreationParams(
                                   MainThreadTaskQueue::QueueType::kTest)))),
        idle_canceled_delayed_taks_sweeper_(
            new IdleCanceledDelayedTaskSweeper(scheduler_helper_.get(),
                                               idle_helper_->IdleTaskRunner())),
        default_task_queue_(scheduler_helper_->DefaultMainThreadTaskQueue()) {
    // Null clock might trigger some assertions.
    task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(5));
  }

  ~IdleCanceledDelayedTaskSweeperTest() override = default;

  void TearDown() override {
    // Check that all tests stop posting tasks.
    task_environment_.FastForwardUntilNoTasksRemain();
  }

  // IdleHelper::Delegate implementation:
  bool CanEnterLongIdlePeriod(
      base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out) override {
    return true;
  }
  void IsNotQuiescent() override {}
  void OnIdlePeriodStarted() override {}
  void OnIdlePeriodEnded() override {}
  void OnPendingTasksChanged(bool has_tasks) override {}

 protected:
  base::test::ScopedTaskEnvironment task_environment_;

  std::unique_ptr<MainThreadSchedulerHelper> scheduler_helper_;
  std::unique_ptr<IdleHelper> idle_helper_;
  std::unique_ptr<IdleCanceledDelayedTaskSweeper>
      idle_canceled_delayed_taks_sweeper_;
  scoped_refptr<base::sequence_manager::TaskQueue> default_task_queue_;

  DISALLOW_COPY_AND_ASSIGN(IdleCanceledDelayedTaskSweeperTest);
};

TEST_F(IdleCanceledDelayedTaskSweeperTest, TestSweep) {
  TestClass class1;
  TestClass class2;

  // Post one task we won't cancel.
  default_task_queue_->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestClass::NopTask, class1.weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(100));

  // And a bunch we will.
  default_task_queue_->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestClass::NopTask, class2.weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(101));

  default_task_queue_->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestClass::NopTask, class2.weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(102));

  default_task_queue_->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestClass::NopTask, class2.weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(103));

  default_task_queue_->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestClass::NopTask, class2.weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(104));

  // Cancel the last four tasks.
  class2.weak_factory_.InvalidateWeakPtrs();

  // Give the IdleCanceledDelayedTaskSweeper a chance to run but don't let
  // the first non canceled delayed task run.  This is important because the
  // canceled tasks would get removed by TaskQueueImpl::WakeUpForDelayedWork.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(40));
  idle_helper_->EnableLongIdlePeriod();
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(40));

  EXPECT_EQ(1u, default_task_queue_->GetNumberOfPendingTasks());
}

}  // namespace scheduler
}  // namespace blink
