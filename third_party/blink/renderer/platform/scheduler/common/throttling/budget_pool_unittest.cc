// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"

#include <stddef.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/null_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/cpu_time_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/wake_up_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

class BudgetPoolTest : public testing::Test {
 public:
  BudgetPoolTest() = default;
  ~BudgetPoolTest() override = default;

  void SetUp() override {
    clock_.Advance(base::TimeDelta::FromMicroseconds(5000));
    null_task_runner_ = base::MakeRefCounted<base::NullTaskRunner>();
    scheduler_.reset(new MainThreadSchedulerImpl(
        base::sequence_manager::SequenceManagerForTest::Create(
            nullptr, null_task_runner_, &clock_),
        base::nullopt));
    task_queue_throttler_ = scheduler_->task_queue_throttler();
    start_time_ = clock_.NowTicks();
  }

  void TearDown() override {
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  base::TimeTicks MillisecondsAfterStart(int milliseconds) {
    return start_time_ + base::TimeDelta::FromMilliseconds(milliseconds);
  }

  base::TimeTicks SecondsAfterStart(int seconds) {
    return start_time_ + base::TimeDelta::FromSeconds(seconds);
  }

 protected:
  base::SimpleTestTickClock clock_;
  scoped_refptr<base::NullTaskRunner> null_task_runner_;
  std::unique_ptr<MainThreadSchedulerImpl> scheduler_;
  TaskQueueThrottler* task_queue_throttler_;  // NOT OWNED
  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(BudgetPoolTest);
};

TEST_F(BudgetPoolTest, CPUTimeBudgetPool) {
  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(SecondsAfterStart(0), 0.1);

  EXPECT_TRUE(pool->CanRunTasksAt(SecondsAfterStart(0), false));
  EXPECT_EQ(SecondsAfterStart(0),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));

  // Run an expensive task and make sure that we're throttled.
  pool->RecordTaskRunTime(nullptr, SecondsAfterStart(0),
                          MillisecondsAfterStart(100));

  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(500), false));
  EXPECT_EQ(MillisecondsAfterStart(1000),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(1000), false));

  // Run a cheap task and make sure that it doesn't affect anything.
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(2000), false));
  pool->RecordTaskRunTime(nullptr, MillisecondsAfterStart(2000),
                          MillisecondsAfterStart(2020));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(2020), false));
  EXPECT_EQ(MillisecondsAfterStart(2020),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));

  pool->Close();
}

TEST_F(BudgetPoolTest, CPUTimeBudgetPoolMinBudgetLevelToRun) {
  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetMinBudgetLevelToRun(SecondsAfterStart(0),
                               base::TimeDelta::FromMilliseconds(10));
  pool->SetTimeBudgetRecoveryRate(SecondsAfterStart(0), 0.1);

  EXPECT_TRUE(pool->CanRunTasksAt(SecondsAfterStart(0), false));
  EXPECT_EQ(SecondsAfterStart(0),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));

  pool->RecordTaskRunTime(nullptr, SecondsAfterStart(0),
                          MillisecondsAfterStart(10));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(15), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(150), false));
  // We need to wait extra 100ms to get budget of 10ms.
  EXPECT_EQ(MillisecondsAfterStart(200),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));

  pool->RecordTaskRunTime(nullptr, MillisecondsAfterStart(200),
                          MillisecondsAfterStart(205));
  // We can run when budget is non-negative even when it less than 10ms.
  EXPECT_EQ(MillisecondsAfterStart(205),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));

  pool->RecordTaskRunTime(nullptr, MillisecondsAfterStart(205),
                          MillisecondsAfterStart(215));
  EXPECT_EQ(MillisecondsAfterStart(350),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));
}

TEST_F(BudgetPoolTest, WakeUpBudgetPool) {
  WakeUpBudgetPool* pool =
      task_queue_throttler_->CreateWakeUpBudgetPool("test");

  scoped_refptr<base::sequence_manager::TaskQueue> queue =
      scheduler_->NewTaskQueueForTest();

  pool->SetWakeUpInterval(base::TimeTicks(), base::TimeDelta::FromSeconds(10));
  pool->SetWakeUpDuration(base::TimeDelta::FromMilliseconds(10));

  // Can't run tasks until a wake-up.
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(0), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(5), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(9), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(10), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(11), false));

  pool->OnWakeUp(MillisecondsAfterStart(0));

  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(0), false));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(5), false));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(9), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(10), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(11), false));

  // GetNextAllowedRunTime should return the desired time when in the
  // wakeup window and return the next wakeup otherwise.
  EXPECT_EQ(start_time_, pool->GetNextAllowedRunTime(start_time_));
  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSeconds(10),
            pool->GetNextAllowedRunTime(MillisecondsAfterStart(15)));

  pool->RecordTaskRunTime(queue.get(), MillisecondsAfterStart(5),
                          MillisecondsAfterStart(7));

  // Make sure that nothing changes after a task inside wakeup window.
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(0), false));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(5), false));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(9), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(10), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(11), false));
  EXPECT_EQ(start_time_, pool->GetNextAllowedRunTime(start_time_));
  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSeconds(10),
            pool->GetNextAllowedRunTime(MillisecondsAfterStart(15)));

  pool->OnWakeUp(MillisecondsAfterStart(12005));
  pool->RecordTaskRunTime(queue.get(), MillisecondsAfterStart(12005),
                          MillisecondsAfterStart(12007));

  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(12005), false));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(12007), false));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(12014), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(12015), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(12016), false));
  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSeconds(20),
            pool->GetNextAllowedRunTime(SecondsAfterStart(13)));
}

}  // namespace scheduler
}  // namespace blink
