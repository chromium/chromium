// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/auto_advancing_virtual_time_domain.h"

#include <memory>
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/message_loop/message_pump.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/test/test_task_time_observer.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_helper.h"

namespace blink {
namespace scheduler {
// Namespace to avoid symbol collisions in jumbo builds.
namespace auto_advancing_virtual_time_domain_unittest {

class AutoAdvancingVirtualTimeDomainTest : public testing::Test {
 public:
  AutoAdvancingVirtualTimeDomainTest() = default;
  ~AutoAdvancingVirtualTimeDomainTest() override = default;

  void SetUp() override {
    sequence_manager_ =
        base::sequence_manager::CreateSequenceManagerOnCurrentThreadWithPump(
            base::MessagePump::Create(base::MessagePumpType::DEFAULT),
            base::sequence_manager::SequenceManager::Settings::Builder()
                .SetMessagePumpType(base::MessagePumpType::DEFAULT)
                .SetPrioritySettings(CreatePrioritySettings())
                .Build());
    scheduler_helper_ = std::make_unique<NonMainThreadSchedulerHelper>(
        sequence_manager_.get(), nullptr, TaskType::kInternalTest);
    scheduler_helper_->AttachToCurrentThread();

    scheduler_helper_->AddTaskTimeObserver(&test_task_time_observer_);
    task_queue_ = scheduler_helper_->DefaultNonMainThreadTaskQueue();
    initial_time_ = base::Time::FromSecondsSinceUnixEpoch(100);
    initial_time_ticks_ = base::TimeTicks() + base::Milliseconds(5);
    auto_advancing_time_domain_ =
        std::make_unique<AutoAdvancingVirtualTimeDomain>(
            initial_time_, initial_time_ticks_, scheduler_helper_.get());
    scheduler_helper_->SetTimeDomain(auto_advancing_time_domain_.get());
  }

  void TearDown() override {
    scheduler_helper_->RemoveTaskTimeObserver(&test_task_time_observer_);
    task_queue_ = scheduler_helper_->DefaultNonMainThreadTaskQueue();
    task_queue_->ShutdownTaskQueue();
    scheduler_helper_->ResetTimeDomain();
  }

  base::Time initial_time_;
  base::TimeTicks initial_time_ticks_;
  std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager_;
  std::unique_ptr<NonMainThreadSchedulerHelper> scheduler_helper_;
  scoped_refptr<NonMainThreadTaskQueue> task_queue_;
  std::unique_ptr<AutoAdvancingVirtualTimeDomain> auto_advancing_time_domain_;
  base::sequence_manager::TestTaskTimeObserver test_task_time_observer_;
};

namespace {
void NopTask(bool* task_run) {
  *task_run = true;
}

}  // namespace

namespace {
void RepostingTask(scoped_refptr<NonMainThreadTaskQueue> task_queue,
                   int max_count,
                   int* count) {
  if (++(*count) >= max_count)
    return;

  task_queue->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&RepostingTask, task_queue, max_count, count));
}

void DelayedTask(int* count_in, int* count_out) {
  *count_out = *count_in;
}

}  // namespace

TEST_F(AutoAdvancingVirtualTimeDomainTest,
       MaxVirtualTimeTaskStarvationCountOneHundred) {
  auto_advancing_time_domain_->SetCanAdvanceVirtualTime(true);
  auto_advancing_time_domain_->SetMaxVirtualTimeTaskStarvationCount(100);

  int count = 0;
  int delayed_task_run_at_count = 0;
  RepostingTask(task_queue_, 1000, &count);
  task_queue_->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(DelayedTask, &count, &delayed_task_run_at_count),
      base::Milliseconds(10));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1000, count);
  EXPECT_EQ(102, delayed_task_run_at_count);
}

TEST_F(AutoAdvancingVirtualTimeDomainTest,
       MaxVirtualTimeTaskStarvationCountZero) {
  auto_advancing_time_domain_->SetCanAdvanceVirtualTime(true);
  auto_advancing_time_domain_->SetMaxVirtualTimeTaskStarvationCount(0);

  int count = 0;
  int delayed_task_run_at_count = 0;
  RepostingTask(task_queue_, 1000, &count);
  task_queue_->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(DelayedTask, &count, &delayed_task_run_at_count),
      base::Milliseconds(10));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1000, count);
  // If the initial count had been higher, the delayed task could have been
  // arbitrarily delayed.
  EXPECT_EQ(1000, delayed_task_run_at_count);
}

TEST_F(AutoAdvancingVirtualTimeDomainTest, TaskStarvationCountIncrements) {
  auto_advancing_time_domain_->SetMaxVirtualTimeTaskStarvationCount(100);
  EXPECT_EQ(0, auto_advancing_time_domain_->task_starvation_count());
  base::PendingTask fake_task(FROM_HERE, base::OnceClosure());
  auto_advancing_time_domain_->DidProcessTask(fake_task);
  EXPECT_EQ(1, auto_advancing_time_domain_->task_starvation_count());
}

TEST_F(AutoAdvancingVirtualTimeDomainTest, TaskStarvationCountNotIncrements) {
  auto_advancing_time_domain_->SetMaxVirtualTimeTaskStarvationCount(0);
  EXPECT_EQ(0, auto_advancing_time_domain_->task_starvation_count());
  base::PendingTask fake_task(FROM_HERE, base::OnceClosure());
  auto_advancing_time_domain_->DidProcessTask(fake_task);
  EXPECT_EQ(0, auto_advancing_time_domain_->task_starvation_count());
}

TEST_F(AutoAdvancingVirtualTimeDomainTest, TaskStarvationCountResets) {
  auto_advancing_time_domain_->SetMaxVirtualTimeTaskStarvationCount(100);
  base::PendingTask fake_task(FROM_HERE, base::OnceClosure());
  auto_advancing_time_domain_->DidProcessTask(fake_task);
  EXPECT_EQ(1, auto_advancing_time_domain_->task_starvation_count());
  auto_advancing_time_domain_->SetMaxVirtualTimeTaskStarvationCount(0);
  EXPECT_EQ(0, auto_advancing_time_domain_->task_starvation_count());
}

TEST_F(AutoAdvancingVirtualTimeDomainTest, BaseTimeOverriden) {
  base::Time initial_time = base::Time::FromSecondsSinceUnixEpoch(100);
  EXPECT_EQ(base::Time::Now(), initial_time);

  // Make time advance.
  base::TimeDelta delay = base::Milliseconds(10);
  bool task_run = false;
  task_queue_->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE, base::BindOnce(NopTask, &task_run), delay);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::Time::Now(), initial_time + delay);
}

TEST_F(AutoAdvancingVirtualTimeDomainTest, BaseTimeTicksOverriden) {
  EXPECT_EQ(base::TimeTicks::Now(), initial_time_ticks_);

  // Make time advance.
  base::TimeDelta delay = base::Milliseconds(20);
  bool task_run = false;
  task_queue_->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE, base::BindOnce(NopTask, &task_run), delay);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::TimeTicks::Now(), initial_time_ticks_ + delay);
  EXPECT_TRUE(task_run);
}

TEST_F(AutoAdvancingVirtualTimeDomainTest, GetNextWakeUpHandlesPastRunTime) {
  // Post a task for t+10ms.
  bool task_run = false;
  task_queue_->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE, base::BindOnce(NopTask, &task_run), base::Milliseconds(10));

  // Advance virtual time past task time to t+100ms.
  auto_advancing_time_domain_->MaybeAdvanceVirtualTime(initial_time_ticks_ +
                                                       base::Milliseconds(100));

  // Task at t+10ms should be run immediately.
  EXPECT_GE(base::TimeTicks::Now(),
            sequence_manager_->GetNextDelayedWakeUp()->time);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(task_run);
}

}  // namespace auto_advancing_virtual_time_domain_unittest
}  // namespace scheduler
}  // namespace blink
