// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/auto_advancing_virtual_time_domain.h"

#include <memory>
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/sequence_manager/test/test_task_queue.h"
#include "base/task/sequence_manager/test/test_task_time_observer.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
    test_task_runner_ = base::WrapRefCounted(new base::TestMockTimeTaskRunner(
        base::TestMockTimeTaskRunner::Type::kBoundToThread));
    // A null clock triggers some assertions.
    test_task_runner_->AdvanceMockTickClock(
        base::TimeDelta::FromMilliseconds(5));
    sequence_manager_ = base::sequence_manager::SequenceManagerForTest::Create(
        nullptr, test_task_runner_, test_task_runner_->GetMockTickClock());
    scheduler_helper_.reset(new NonMainThreadSchedulerHelper(
        sequence_manager_.get(), nullptr, TaskType::kInternalTest));

    scheduler_helper_->AddTaskTimeObserver(&test_task_time_observer_);
    task_queue_ = scheduler_helper_->DefaultNonMainThreadTaskQueue();
    initial_time_ = base::Time::FromJsTime(100000.0);
    initial_time_ticks_ = test_task_runner_->NowTicks();
    auto_advancing_time_domain_.reset(new AutoAdvancingVirtualTimeDomain(
        initial_time_, initial_time_ticks_, scheduler_helper_.get(),
        AutoAdvancingVirtualTimeDomain::BaseTimeOverridePolicy::OVERRIDE));
    scheduler_helper_->RegisterTimeDomain(auto_advancing_time_domain_.get());
    task_queue_->SetTimeDomain(auto_advancing_time_domain_.get());
  }

  void TearDown() override {
    task_queue_->ShutdownTaskQueue();
    scheduler_helper_->UnregisterTimeDomain(auto_advancing_time_domain_.get());
  }

  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  base::Time initial_time_;
  base::TimeTicks initial_time_ticks_;
  std::unique_ptr<base::sequence_manager::SequenceManagerForTest>
      sequence_manager_;
  std::unique_ptr<NonMainThreadSchedulerHelper> scheduler_helper_;
  scoped_refptr<base::sequence_manager::TaskQueue> task_queue_;
  std::unique_ptr<AutoAdvancingVirtualTimeDomain> auto_advancing_time_domain_;
  base::sequence_manager::TestTaskTimeObserver test_task_time_observer_;
};

namespace {
void NopTask(bool* task_run) {
  *task_run = true;
}

}  // namespace

namespace {
void RepostingTask(scoped_refptr<base::sequence_manager::TaskQueue> task_queue,
                   int max_count,
                   int* count) {
  if (++(*count) >= max_count)
    return;

  task_queue->task_runner()->PostTask(
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
  task_queue_->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(DelayedTask, &count, &delayed_task_run_at_count),
      base::TimeDelta::FromMilliseconds(10));

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
  task_queue_->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(DelayedTask, &count, &delayed_task_run_at_count),
      base::TimeDelta::FromMilliseconds(10));

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
  base::Time initial_time = base::Time::FromJsTime(100000.0);
  EXPECT_EQ(base::Time::Now(), initial_time);

  // Make time advance.
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  bool task_run = false;
  task_queue_->task_runner()->PostDelayedTask(
      FROM_HERE, base::BindOnce(NopTask, &task_run), delay);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::Time::Now(), initial_time + delay);
}

TEST_F(AutoAdvancingVirtualTimeDomainTest, BaseTimeTicksOverriden) {
  base::TimeTicks initial_time = test_task_runner_->NowTicks();
  EXPECT_EQ(base::TimeTicks::Now(), initial_time);

  // Make time advance.
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(20);
  bool task_run = false;
  task_queue_->task_runner()->PostDelayedTask(
      FROM_HERE, base::BindOnce(NopTask, &task_run), delay);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::TimeTicks::Now(), initial_time + delay);
}

TEST_F(AutoAdvancingVirtualTimeDomainTest,
       DelayTillNextTaskHandlesPastRunTime) {
  base::TimeTicks initial_time = test_task_runner_->NowTicks();

  // Post a task for t+10ms.
  bool task_run = false;
  task_queue_->task_runner()->PostDelayedTask(
      FROM_HERE, base::BindOnce(NopTask, &task_run),
      base::TimeDelta::FromMilliseconds(10));

  // Advance virtual time past task time to t+100ms.
  auto_advancing_time_domain_->MaybeAdvanceVirtualTime(
      initial_time + base::TimeDelta::FromMilliseconds(100));

  // Task at t+10ms should be run immediately.
  EXPECT_EQ(base::TimeDelta(),
            auto_advancing_time_domain_->DelayTillNextTask(nullptr));
}

}  // namespace auto_advancing_virtual_time_domain_unittest
}  // namespace scheduler
}  // namespace blink
