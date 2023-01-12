// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"

#include <stddef.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/null_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
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
  BudgetPoolTest(const BudgetPoolTest&) = delete;
  BudgetPoolTest& operator=(const BudgetPoolTest&) = delete;
  ~BudgetPoolTest() override = default;

  void SetUp() override {
    clock_.Advance(base::Microseconds(5000));
    null_task_runner_ = base::MakeRefCounted<base::NullTaskRunner>();
    start_time_ = clock_.NowTicks();
  }

  base::TimeTicks MillisecondsAfterStart(int milliseconds) {
    return start_time_ + base::Milliseconds(milliseconds);
  }

  base::TimeTicks SecondsAfterStart(int seconds) {
    return start_time_ + base::Seconds(seconds);
  }

 protected:
  base::SimpleTestTickClock clock_;
  scoped_refptr<base::NullTaskRunner> null_task_runner_;
  TraceableVariableController tracing_controller_;
  base::TimeTicks start_time_;
};

TEST_F(BudgetPoolTest, CPUTimeBudgetPool) {
  std::unique_ptr<CPUTimeBudgetPool> pool = std::make_unique<CPUTimeBudgetPool>(
      "test", &tracing_controller_, start_time_);

  pool->SetTimeBudgetRecoveryRate(SecondsAfterStart(0), 0.1);

  EXPECT_TRUE(pool->CanRunTasksAt(SecondsAfterStart(0)));
  EXPECT_EQ(SecondsAfterStart(0),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));

  // Run an expensive task and make sure that we're throttled.
  pool->RecordTaskRunTime(SecondsAfterStart(0), MillisecondsAfterStart(100));

  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(500)));
  EXPECT_EQ(MillisecondsAfterStart(1000),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(1000)));

  // Run a cheap task and make sure that it doesn't affect anything.
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(2000)));
  pool->RecordTaskRunTime(MillisecondsAfterStart(2000),
                          MillisecondsAfterStart(2020));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(2020)));
  EXPECT_EQ(MillisecondsAfterStart(2020),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));
}

TEST_F(BudgetPoolTest, WakeUpBudgetPool) {
  std::unique_ptr<WakeUpBudgetPool> pool =
      std::make_unique<WakeUpBudgetPool>("test");

  pool->SetWakeUpInterval(base::TimeTicks(), base::Seconds(10));
  pool->SetWakeUpDuration(base::Milliseconds(10));

  // Can't run tasks until a wake-up.
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(0)));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(5)));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(9)));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(10)));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(11)));

  pool->OnWakeUp(MillisecondsAfterStart(0));

  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(0)));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(5)));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(9)));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(10)));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(11)));

  // GetNextAllowedRunTime should return the desired time when in the
  // wakeup window and return the next wakeup otherwise.
  EXPECT_EQ(start_time_, pool->GetNextAllowedRunTime(start_time_));
  EXPECT_EQ(base::TimeTicks() + base::Seconds(10),
            pool->GetNextAllowedRunTime(MillisecondsAfterStart(15)));

  pool->RecordTaskRunTime(MillisecondsAfterStart(5), MillisecondsAfterStart(7));

  // Make sure that nothing changes after a task inside wakeup window.
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(0)));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(5)));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(9)));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(10)));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(11)));
  EXPECT_EQ(start_time_, pool->GetNextAllowedRunTime(start_time_));
  EXPECT_EQ(base::TimeTicks() + base::Seconds(10),
            pool->GetNextAllowedRunTime(MillisecondsAfterStart(15)));

  pool->OnWakeUp(MillisecondsAfterStart(12005));
  pool->RecordTaskRunTime(MillisecondsAfterStart(12005),
                          MillisecondsAfterStart(12007));

  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(12005)));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(12007)));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(12014)));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(12015)));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(12016)));
  EXPECT_EQ(base::TimeTicks() + base::Seconds(20),
            pool->GetNextAllowedRunTime(SecondsAfterStart(13)));
}

}  // namespace scheduler
}  // namespace blink
