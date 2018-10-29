// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"

#include <stddef.h>

#include <deque>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/auto_advancing_virtual_time_domain.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace task_queue_throttler_unittest {

using base::TimeDelta;
using base::TimeTicks;
using base::TestMockTimeTaskRunner;
using base::sequence_manager::LazyNow;
using base::sequence_manager::TaskQueue;
using testing::ElementsAre;

class MainThreadSchedulerImplForTest : public MainThreadSchedulerImpl {
 public:
  using MainThreadSchedulerImpl::ControlTaskQueue;

  MainThreadSchedulerImplForTest(
      std::unique_ptr<base::sequence_manager::SequenceManager> manager,
      base::Optional<base::Time> initial_virtual_time)
      : MainThreadSchedulerImpl(std::move(manager), initial_virtual_time) {}
};

void NopTask() {}

void AddOneTask(size_t* count) {
  (*count)++;
}

void RunTenTimesTask(size_t* count, scoped_refptr<TaskQueue> timer_queue) {
  if (++(*count) < 10) {
    timer_queue->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&RunTenTimesTask, count, timer_queue));
  }
}

class TaskQueueThrottlerTest : public testing::Test {
 public:
  TaskQueueThrottlerTest()
      : test_task_runner_(base::MakeRefCounted<TestMockTimeTaskRunner>()) {}
  ~TaskQueueThrottlerTest() override = default;

  void SetUp() override {
    // A null clock triggers some assertions.
    test_task_runner_->AdvanceMockTickClock(TimeDelta::FromMilliseconds(5));

    scheduler_.reset(new MainThreadSchedulerImplForTest(
        base::sequence_manager::SequenceManagerForTest::Create(
            nullptr, test_task_runner_, GetTickClock()),
        base::nullopt));
    scheduler_->GetWakeUpBudgetPoolForTesting()->SetWakeUpDuration(TimeDelta());
    task_queue_throttler_ = scheduler_->task_queue_throttler();
    timer_queue_ = scheduler_->NewTimerTaskQueue(
        MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr);
    timer_task_runner_ = timer_queue_->task_runner();
  }

  void TearDown() override {
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  void ExpectThrottled(scoped_refptr<TaskQueue> timer_queue) {
    size_t count = 0;
    timer_queue->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&RunTenTimesTask, &count, timer_queue));

    test_task_runner_->FastForwardBy(TimeDelta::FromSeconds(1));
    EXPECT_LE(count, 1u);

    // Make sure the rest of the tasks run or we risk a UAF on |count|.
    test_task_runner_->FastForwardBy(TimeDelta::FromSeconds(10));
    EXPECT_EQ(10u, count);
  }

  void ExpectUnthrottled(scoped_refptr<TaskQueue> timer_queue) {
    size_t count = 0;
    timer_queue->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&RunTenTimesTask, &count, timer_queue));

    test_task_runner_->FastForwardBy(TimeDelta::FromSeconds(1));
    EXPECT_EQ(10u, count);
    test_task_runner_->FastForwardUntilNoTasksRemain();
  }

  bool IsQueueBlocked(TaskQueue* task_queue) {
    if (!task_queue->IsQueueEnabled())
      return true;
    return task_queue->BlockedByFence();
  }

 protected:
  virtual const base::TickClock* GetTickClock() const {
    return test_task_runner_->GetMockTickClock();
  }

  scoped_refptr<TestMockTimeTaskRunner> test_task_runner_;
  std::unique_ptr<MainThreadSchedulerImplForTest> scheduler_;
  scoped_refptr<TaskQueue> timer_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner_;
  TaskQueueThrottler* task_queue_throttler_;  // NOT OWNED

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskQueueThrottlerTest);
};

// Advances mock clock every time we call NowTicks() from the scheduler.
class AutoAdvancingProxyClock : public base::TickClock {
 public:
  AutoAdvancingProxyClock(scoped_refptr<TestMockTimeTaskRunner> task_runner)
      : task_runner_(task_runner) {}
  ~AutoAdvancingProxyClock() override = default;

  void SetAutoAdvanceInterval(TimeDelta interval) {
    advance_interval_ = interval;
  }

  TimeTicks NowTicks() const override {
    if (!advance_interval_.is_zero())
      task_runner_->AdvanceMockTickClock(advance_interval_);
    return task_runner_->NowTicks();
  }

 private:
  scoped_refptr<TestMockTimeTaskRunner> task_runner_;
  TimeDelta advance_interval_;
};

class TaskQueueThrottlerWithAutoAdvancingTimeTest
    : public TaskQueueThrottlerTest,
      public testing::WithParamInterface<bool> {
 public:
  TaskQueueThrottlerWithAutoAdvancingTimeTest()
      : proxy_clock_(test_task_runner_) {}
  ~TaskQueueThrottlerWithAutoAdvancingTimeTest() override = default;

  void SetUp() override {
    TaskQueueThrottlerTest::SetUp();
    if (GetParam()) {
      // Will advance the time by this value after running each task.
      proxy_clock_.SetAutoAdvanceInterval(TimeDelta::FromMicroseconds(10));
    }
  }

 protected:
  const base::TickClock* GetTickClock() const override { return &proxy_clock_; }

 private:
  AutoAdvancingProxyClock proxy_clock_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueThrottlerWithAutoAdvancingTimeTest);
};

INSTANTIATE_TEST_CASE_P(,
                        TaskQueueThrottlerWithAutoAdvancingTimeTest,
                        testing::Bool());

TEST_F(TaskQueueThrottlerTest, ThrottledTasksReportRealTime) {
  EXPECT_EQ(timer_queue_->GetTimeDomain()->Now(),
            test_task_runner_->NowTicks());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_EQ(timer_queue_->GetTimeDomain()->Now(),
            test_task_runner_->NowTicks());

  test_task_runner_->AdvanceMockTickClock(TimeDelta::FromMilliseconds(250));
  // Make sure the throttled time domain's Now() reports the same as the
  // underlying clock.
  EXPECT_EQ(timer_queue_->GetTimeDomain()->Now(),
            test_task_runner_->NowTicks());
}

TEST_F(TaskQueueThrottlerTest, AlignedThrottledRunTime) {
  EXPECT_EQ(TimeTicks() + TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                TimeTicks() + TimeDelta::FromSecondsD(0.0)));

  EXPECT_EQ(TimeTicks() + TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                TimeTicks() + TimeDelta::FromSecondsD(0.1)));

  EXPECT_EQ(TimeTicks() + TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                TimeTicks() + TimeDelta::FromSecondsD(0.2)));

  EXPECT_EQ(TimeTicks() + TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                TimeTicks() + TimeDelta::FromSecondsD(0.5)));

  EXPECT_EQ(TimeTicks() + TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                TimeTicks() + TimeDelta::FromSecondsD(0.8)));

  EXPECT_EQ(TimeTicks() + TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                TimeTicks() + TimeDelta::FromSecondsD(0.9)));

  EXPECT_EQ(TimeTicks() + TimeDelta::FromSecondsD(2.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                TimeTicks() + TimeDelta::FromSecondsD(1.0)));

  EXPECT_EQ(TimeTicks() + TimeDelta::FromSecondsD(2.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                TimeTicks() + TimeDelta::FromSecondsD(1.1)));

  EXPECT_EQ(TimeTicks() + TimeDelta::FromSecondsD(9.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                TimeTicks() + TimeDelta::FromSecondsD(8.0)));

  EXPECT_EQ(TimeTicks() + TimeDelta::FromSecondsD(9.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                TimeTicks() + TimeDelta::FromSecondsD(8.1)));
}

namespace {

// Round up time to milliseconds to deal with autoadvancing time.
// TODO(altimin): round time only when autoadvancing time is enabled.
TimeDelta RoundTimeToMilliseconds(TimeDelta time) {
  return time - time % TimeDelta::FromMilliseconds(1);
}

TimeTicks RoundTimeToMilliseconds(TimeTicks time) {
  return TimeTicks() + RoundTimeToMilliseconds(time - TimeTicks());
}

void TestTask(std::vector<TimeTicks>* run_times,
              scoped_refptr<TestMockTimeTaskRunner> task_runner) {
  run_times->push_back(RoundTimeToMilliseconds(task_runner->NowTicks()));
  // FIXME No auto-advancing
}

void ExpensiveTestTask(std::vector<TimeTicks>* run_times,
                       scoped_refptr<TestMockTimeTaskRunner> task_runner) {
  run_times->push_back(RoundTimeToMilliseconds(task_runner->NowTicks()));
  task_runner->AdvanceMockTickClock(TimeDelta::FromMilliseconds(250));
  // FIXME No auto-advancing
}

void RecordThrottling(std::vector<TimeDelta>* reported_throttling_times,
                      TimeDelta throttling_duration) {
  reported_throttling_times->push_back(
      RoundTimeToMilliseconds(throttling_duration));
}
}  // namespace

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, TimerAlignment) {
  std::vector<TimeTicks> run_times;
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200.0));

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(800.0));

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(1200.0));

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(8300.0));

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // Times are aligned to a multiple of 1000 milliseconds.
  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(1000.0),
                          TimeTicks() + TimeDelta::FromMilliseconds(1000.0),
                          TimeTicks() + TimeDelta::FromMilliseconds(2000.0),
                          TimeTicks() + TimeDelta::FromMilliseconds(9000.0)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       TimerAlignment_Unthrottled) {
  std::vector<TimeTicks> run_times;
  TimeTicks start_time = test_task_runner_->NowTicks();
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200.0));

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(800.0));

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(1200.0));

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(8300.0));

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // Times are not aligned.
  EXPECT_THAT(
      run_times,
      ElementsAre(RoundTimeToMilliseconds(start_time +
                                          TimeDelta::FromMilliseconds(200.0)),
                  RoundTimeToMilliseconds(start_time +
                                          TimeDelta::FromMilliseconds(800.0)),
                  RoundTimeToMilliseconds(start_time +
                                          TimeDelta::FromMilliseconds(1200.0)),
                  RoundTimeToMilliseconds(
                      start_time + TimeDelta::FromMilliseconds(8300.0))));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, Refcount) {
  ExpectUnthrottled(timer_queue_.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  ExpectThrottled(timer_queue_);

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  ExpectThrottled(timer_queue_);

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  ExpectThrottled(timer_queue_);

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  ExpectUnthrottled(timer_queue_);

  // Should be a NOP.
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  ExpectUnthrottled(timer_queue_);

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  ExpectThrottled(timer_queue_);
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       ThrotlingAnEmptyQueueDoesNotPostPumpThrottledTasksLocked) {
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  EXPECT_TRUE(scheduler_->ControlTaskQueue()->IsEmpty());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       OnTimeDomainHasImmediateWork_EnabledQueue) {
  task_queue_throttler_->OnQueueNextWakeUpChanged(timer_queue_.get(),
                                                  TimeTicks());
  // Check PostPumpThrottledTasksLocked was called.
  EXPECT_FALSE(scheduler_->ControlTaskQueue()->IsEmpty());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       OnTimeDomainHasImmediateWork_DisabledQueue) {
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      timer_queue_->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);

  task_queue_throttler_->OnQueueNextWakeUpChanged(timer_queue_.get(),
                                                  TimeTicks());
  // Check PostPumpThrottledTasksLocked was not called.
  EXPECT_TRUE(scheduler_->ControlTaskQueue()->IsEmpty());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       ThrottlingADisabledQueueDoesNotPostPumpThrottledTasks) {
  timer_task_runner_->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      timer_queue_->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_TRUE(scheduler_->ControlTaskQueue()->IsEmpty());

  // Enabling it should trigger a call to PostPumpThrottledTasksLocked.
  voter->SetQueueEnabled(true);
  EXPECT_FALSE(scheduler_->ControlTaskQueue()->IsEmpty());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       ThrottlingADisabledQueueDoesNotPostPumpThrottledTasks_DelayedTask) {
  timer_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                                      TimeDelta::FromMilliseconds(1));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      timer_queue_->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_TRUE(scheduler_->ControlTaskQueue()->IsEmpty());

  // Enabling it should trigger a call to PostPumpThrottledTasksLocked.
  voter->SetQueueEnabled(true);
  EXPECT_FALSE(scheduler_->ControlTaskQueue()->IsEmpty());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, WakeUpForNonDelayedTask) {
  std::vector<TimeTicks> run_times;

  // Nothing is posted on timer_queue_ so PumpThrottledTasks will not tick.
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Posting a task should trigger the pump.
  timer_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_));

  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(1000.0)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, WakeUpForDelayedTask) {
  std::vector<TimeTicks> run_times;

  // Nothing is posted on timer_queue_ so PumpThrottledTasks will not tick.
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Posting a task should trigger the pump.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(1200.0));

  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(2000.0)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       SingleThrottledTaskPumpedAndRunWithNoExtraneousMessageLoopTasks) {
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  TimeDelta delay(TimeDelta::FromMilliseconds(10));
  timer_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                                      delay);
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       SingleFutureThrottledTaskPumpedAndRunWithNoExtraneousMessageLoopTasks) {
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  TimeDelta delay(TimeDelta::FromSecondsD(15.5));
  timer_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                                      delay);
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       TwoFutureThrottledTaskPumpedAndRunWithNoExtraneousMessageLoopTasks) {
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  std::vector<TimeTicks> run_times;

  TimeDelta delay(TimeDelta::FromSecondsD(15.5));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      delay);

  TimeDelta delay2(TimeDelta::FromSecondsD(5.5));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      delay2);

  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  test_task_runner_->FastForwardBy(test_task_runner_->NextPendingTaskDelay());
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  test_task_runner_->FastForwardBy(test_task_runner_->NextPendingTaskDelay());
  EXPECT_EQ(0u, test_task_runner_->GetPendingTaskCount());

  EXPECT_THAT(run_times, ElementsAre(TimeTicks() + TimeDelta::FromSeconds(6),
                                     TimeTicks() + TimeDelta::FromSeconds(16)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       TaskDelayIsBasedOnRealTime) {
  std::vector<TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Post an initial task that should run at the first aligned time period.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(900.0));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // Advance realtime.
  test_task_runner_->AdvanceMockTickClock(TimeDelta::FromMilliseconds(250));

  // Post a task that due to real time + delay must run in the third aligned
  // time period.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(900.0));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(1000.0),
                          TimeTicks() + TimeDelta::FromMilliseconds(3000.0)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, TaskQueueDisabledTillPump) {
  size_t count = 0;
  timer_task_runner_->PostTask(FROM_HERE, base::BindOnce(&AddOneTask, &count));

  EXPECT_FALSE(IsQueueBlocked(timer_queue_.get()));
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_TRUE(IsQueueBlocked(timer_queue_.get()));

  test_task_runner_->FastForwardUntilNoTasksRemain();  // Wait until the pump.
  EXPECT_EQ(1u, count);               // The task got run.
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       DoubleIncrementDoubleDecrement) {
  timer_task_runner_->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  EXPECT_FALSE(IsQueueBlocked(timer_queue_.get()));
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_TRUE(IsQueueBlocked(timer_queue_.get()));
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  EXPECT_FALSE(IsQueueBlocked(timer_queue_.get()));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       EnableVirtualTimeThenIncrement) {
  timer_task_runner_->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  scheduler_->EnableVirtualTime(
      MainThreadSchedulerImpl::BaseTimeOverridePolicy::DO_NOT_OVERRIDE);
  EXPECT_EQ(timer_queue_->GetTimeDomain(), scheduler_->GetVirtualTimeDomain());

  EXPECT_FALSE(IsQueueBlocked(timer_queue_.get()));
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_FALSE(IsQueueBlocked(timer_queue_.get()));
  EXPECT_EQ(timer_queue_->GetTimeDomain(), scheduler_->GetVirtualTimeDomain());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       IncrementThenEnableVirtualTime) {
  timer_task_runner_->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  EXPECT_FALSE(IsQueueBlocked(timer_queue_.get()));
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_TRUE(IsQueueBlocked(timer_queue_.get()));

  scheduler_->EnableVirtualTime(
      MainThreadSchedulerImpl::BaseTimeOverridePolicy::DO_NOT_OVERRIDE);
  EXPECT_FALSE(IsQueueBlocked(timer_queue_.get()));
  EXPECT_EQ(timer_queue_->GetTimeDomain(), scheduler_->GetVirtualTimeDomain());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, TimeBasedThrottling) {
  std::vector<TimeTicks> run_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(TimeTicks(), 0.1);
  pool->AddQueue(TimeTicks(), timer_queue_.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Submit two tasks. They should be aligned, and second one should be
  // throttled.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times, ElementsAre(TimeTicks() + TimeDelta::FromSeconds(1),
                                     TimeTicks() + TimeDelta::FromSeconds(3)));

  pool->RemoveQueue(test_task_runner_->NowTicks(), timer_queue_.get());
  run_times.clear();

  // Queue was removed from CPUTimeBudgetPool, only timer alignment should be
  // active now.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(4000),
                          TimeTicks() + TimeDelta::FromMilliseconds(4250)));

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  pool->Close();
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       EnableAndDisableCPUTimeBudgetPool) {
  std::vector<TimeTicks> run_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");
  EXPECT_TRUE(pool->IsThrottlingEnabled());

  pool->SetTimeBudgetRecoveryRate(TimeTicks(), 0.1);
  pool->AddQueue(TimeTicks(), timer_queue_.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Post an expensive task. Pool is now throttled.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(1000)));
  run_times.clear();

  LazyNow lazy_now_1(test_task_runner_->GetMockTickClock());
  pool->DisableThrottling(&lazy_now_1);
  EXPECT_FALSE(pool->IsThrottlingEnabled());

  // Pool should not be throttled now.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(2000)));
  run_times.clear();

  LazyNow lazy_now_2(test_task_runner_->GetMockTickClock());
  pool->EnableThrottling(&lazy_now_2);
  EXPECT_TRUE(pool->IsThrottlingEnabled());

  // Because time pool was disabled, time budget level did not replenish
  // and queue is throttled.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(4000)));
  run_times.clear();

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());

  pool->RemoveQueue(test_task_runner_->NowTicks(), timer_queue_.get());
  pool->Close();
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       ImmediateTasksTimeBudgetThrottling) {
  std::vector<TimeTicks> run_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(TimeTicks(), 0.1);
  pool->AddQueue(TimeTicks(), timer_queue_.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Submit two tasks. They should be aligned, and second one should be
  // throttled.
  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));
  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times, ElementsAre(TimeTicks() + TimeDelta::FromSeconds(1),
                                     TimeTicks() + TimeDelta::FromSeconds(3)));

  pool->RemoveQueue(test_task_runner_->NowTicks(), timer_queue_.get());
  run_times.clear();

  // Queue was removed from CPUTimeBudgetPool, only timer alignment should be
  // active now.
  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));
  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(4000),
                          TimeTicks() + TimeDelta::FromMilliseconds(4250)));

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  pool->Close();
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       TwoQueuesTimeBudgetThrottling) {
  std::vector<TimeTicks> run_times;

  scoped_refptr<TaskQueue> second_queue = scheduler_->NewTimerTaskQueue(
      MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr);

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(TimeTicks(), 0.1);
  pool->AddQueue(TimeTicks(), timer_queue_.get());
  pool->AddQueue(TimeTicks(), second_queue.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  task_queue_throttler_->IncreaseThrottleRefCount(second_queue.get());

  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));
  second_queue->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times, ElementsAre(TimeTicks() + TimeDelta::FromSeconds(1),
                                     TimeTicks() + TimeDelta::FromSeconds(3)));

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  task_queue_throttler_->DecreaseThrottleRefCount(second_queue.get());

  pool->RemoveQueue(test_task_runner_->NowTicks(), timer_queue_.get());
  pool->RemoveQueue(test_task_runner_->NowTicks(), second_queue.get());

  pool->Close();
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       DisabledTimeBudgetDoesNotAffectThrottledQueues) {
  std::vector<TimeTicks> run_times;
  LazyNow lazy_now(test_task_runner_->GetMockTickClock());

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");
  pool->SetTimeBudgetRecoveryRate(lazy_now.Now(), 0.1);
  pool->DisableThrottling(&lazy_now);

  pool->AddQueue(lazy_now.Now(), timer_queue_.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(100));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(100));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(1000),
                          TimeTicks() + TimeDelta::FromMilliseconds(1250)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       TimeBudgetThrottlingDoesNotAffectUnthrottledQueues) {
  std::vector<TimeTicks> run_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");
  pool->SetTimeBudgetRecoveryRate(TimeTicks(), 0.1);

  LazyNow lazy_now(test_task_runner_->GetMockTickClock());
  pool->DisableThrottling(&lazy_now);

  pool->AddQueue(test_task_runner_->NowTicks(), timer_queue_.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(100));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(100));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(105),
                          TimeTicks() + TimeDelta::FromMilliseconds(355)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, MaxThrottlingDelay) {
  std::vector<TimeTicks> run_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetMaxThrottlingDelay(TimeTicks(), TimeDelta::FromMinutes(1));

  pool->SetTimeBudgetRecoveryRate(TimeTicks(), 0.001);
  pool->AddQueue(TimeTicks(), timer_queue_.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  for (int i = 0; i < 5; ++i) {
    timer_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
        TimeDelta::FromMilliseconds(200));
  }

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromSeconds(1),
                          TimeTicks() + TimeDelta::FromSeconds(62),
                          TimeTicks() + TimeDelta::FromSeconds(123),
                          TimeTicks() + TimeDelta::FromSeconds(184),
                          TimeTicks() + TimeDelta::FromSeconds(245)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       EnableAndDisableThrottling) {
  std::vector<TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(295));

  // Disable throttling - task should run immediately.
  task_queue_throttler_->DisableThrottling();

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(200));

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(300)));
  run_times.clear();

  // Schedule a task at 900ms. It should proceed as normal.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(400));

  // Schedule a task at 1200ms. It should proceed as normal.
  // PumpThrottledTasks was scheduled at 1000ms, so it needs to be checked
  // that it was cancelled and it does not interfere with tasks posted before
  // 1s mark and scheduled to run after 1s mark.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(700));

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(800));

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(900),
                          TimeTicks() + TimeDelta::FromMilliseconds(1200)));
  run_times.clear();

  // Schedule a task at 1500ms. It should be throttled because of enabled
  // throttling.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(100));

  // Throttling is enabled and new task should be aligned.
  task_queue_throttler_->EnableThrottling();

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(2000)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, ReportThrottling) {
  std::vector<TimeTicks> run_times;
  std::vector<TimeDelta> reported_throttling_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(TimeTicks(), 0.1);
  pool->AddQueue(TimeTicks(), timer_queue_.get());

  pool->SetReportingCallback(
      base::BindRepeating(&RecordThrottling, &reported_throttling_times));

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times, ElementsAre(TimeTicks() + TimeDelta::FromSeconds(1),
                                     TimeTicks() + TimeDelta::FromSeconds(1),
                                     TimeTicks() + TimeDelta::FromSeconds(3)));

  EXPECT_THAT(reported_throttling_times,
              ElementsAre(TimeDelta::FromMilliseconds(1255),
                          TimeDelta::FromMilliseconds(1755)));

  pool->RemoveQueue(test_task_runner_->NowTicks(), timer_queue_.get());
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  pool->Close();
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, GrantAdditionalBudget) {
  std::vector<TimeTicks> run_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(TimeTicks(), 0.1);
  pool->AddQueue(TimeTicks(), timer_queue_.get());
  pool->GrantAdditionalBudget(TimeTicks(), TimeDelta::FromMilliseconds(500));

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Submit five tasks. First three will not be throttled because they have
  // budget to run.
  for (int i = 0; i < 5; ++i) {
    timer_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
        TimeDelta::FromMilliseconds(200));
  }

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(1000),
                          TimeTicks() + TimeDelta::FromMilliseconds(1250),
                          TimeTicks() + TimeDelta::FromMilliseconds(1500),
                          TimeTicks() + TimeDelta::FromSeconds(3),
                          TimeTicks() + TimeDelta::FromSeconds(6)));

  pool->RemoveQueue(test_task_runner_->NowTicks(), timer_queue_.get());
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  pool->Close();
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       EnableAndDisableThrottlingAndTimeBudgets) {
  // This test checks that if time budget pool is enabled when throttling
  // is disabled, it does not throttle the queue.
  std::vector<TimeTicks> run_times;

  task_queue_throttler_->DisableThrottling();

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  LazyNow lazy_now_1(test_task_runner_->GetMockTickClock());
  pool->DisableThrottling(&lazy_now_1);

  pool->AddQueue(TimeTicks(), timer_queue_.get());

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(95));

  LazyNow lazy_now_2(test_task_runner_->GetMockTickClock());
  pool->EnableThrottling(&lazy_now_2);

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(300)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       AddQueueToBudgetPoolWhenThrottlingDisabled) {
  // This test checks that a task queue is added to time budget pool
  // when throttling is disabled, is does not throttle queue.
  std::vector<TimeTicks> run_times;

  task_queue_throttler_->DisableThrottling();

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(95));

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200));

  pool->AddQueue(TimeTicks(), timer_queue_.get());

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(300)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       DisabledQueueThenEnabledQueue) {
  std::vector<TimeTicks> run_times;

  scoped_refptr<MainThreadTaskQueue> second_queue =
      scheduler_->NewTimerTaskQueue(
          MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr);

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  task_queue_throttler_->IncreaseThrottleRefCount(second_queue.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(100));
  second_queue->task_runner()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      TimeDelta::FromMilliseconds(200));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      timer_queue_->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);

  test_task_runner_->AdvanceMockTickClock(TimeDelta::FromMilliseconds(250));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(1000)));

  voter->SetQueueEnabled(true);
  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(1000),
                          TimeTicks() + TimeDelta::FromMilliseconds(2000)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, TwoBudgetPools) {
  std::vector<TimeTicks> run_times;

  scoped_refptr<TaskQueue> second_queue = scheduler_->NewTimerTaskQueue(
      MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr);

  CPUTimeBudgetPool* pool1 =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");
  pool1->SetTimeBudgetRecoveryRate(TimeTicks(), 0.1);
  pool1->AddQueue(TimeTicks(), timer_queue_.get());
  pool1->AddQueue(TimeTicks(), second_queue.get());

  CPUTimeBudgetPool* pool2 =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");
  pool2->SetTimeBudgetRecoveryRate(TimeTicks(), 0.01);
  pool2->AddQueue(TimeTicks(), timer_queue_.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  task_queue_throttler_->IncreaseThrottleRefCount(second_queue.get());

  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));
  second_queue->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));
  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));
  second_queue->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(1000),
                          TimeTicks() + TimeDelta::FromMilliseconds(3000),
                          TimeTicks() + TimeDelta::FromMilliseconds(6000),
                          TimeTicks() + TimeDelta::FromMilliseconds(26000)));
}

namespace {

void RunChainedTask(std::deque<TimeDelta> task_durations,
                    scoped_refptr<TaskQueue> queue,
                    scoped_refptr<TestMockTimeTaskRunner> task_runner,
                    std::vector<TimeTicks>* run_times,
                    TimeDelta delay) {
  if (task_durations.empty())
    return;

  // FIXME No auto-advancing.

  run_times->push_back(RoundTimeToMilliseconds(task_runner->NowTicks()));
  task_runner->AdvanceMockTickClock(task_durations.front());
  task_durations.pop_front();

  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RunChainedTask, std::move(task_durations), queue,
                     task_runner, run_times, delay),
      delay);
}
}  // namespace

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       WakeUpBasedThrottling_ChainedTasks_Instantaneous) {
  scheduler_->GetWakeUpBudgetPoolForTesting()->SetWakeUpDuration(
      TimeDelta::FromMilliseconds(10));
  std::vector<TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RunChainedTask, std::deque<TimeDelta>(10, TimeDelta()),
                     timer_queue_, test_task_runner_, &run_times, TimeDelta()),
      TimeDelta::FromMilliseconds(100));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times, ElementsAre(TimeTicks() + TimeDelta::FromSeconds(1),
                                     TimeTicks() + TimeDelta::FromSeconds(1),
                                     TimeTicks() + TimeDelta::FromSeconds(1),
                                     TimeTicks() + TimeDelta::FromSeconds(1),
                                     TimeTicks() + TimeDelta::FromSeconds(1),
                                     TimeTicks() + TimeDelta::FromSeconds(1),
                                     TimeTicks() + TimeDelta::FromSeconds(1),
                                     TimeTicks() + TimeDelta::FromSeconds(1),
                                     TimeTicks() + TimeDelta::FromSeconds(1),
                                     TimeTicks() + TimeDelta::FromSeconds(1)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       WakeUpBasedThrottling_ImmediateTasks_Fast) {
  scheduler_->GetWakeUpBudgetPoolForTesting()->SetWakeUpDuration(
      TimeDelta::FromMilliseconds(10));
  std::vector<TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RunChainedTask,
                     std::deque<TimeDelta>(10, TimeDelta::FromMilliseconds(3)),
                     timer_queue_, test_task_runner_, &run_times, TimeDelta()),
      TimeDelta::FromMilliseconds(100));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // TODO(altimin): Add fence mechanism to block immediate tasks.
  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(1000),
                          TimeTicks() + TimeDelta::FromMilliseconds(1003),
                          TimeTicks() + TimeDelta::FromMilliseconds(1006),
                          TimeTicks() + TimeDelta::FromMilliseconds(1009),
                          TimeTicks() + TimeDelta::FromMilliseconds(2000),
                          TimeTicks() + TimeDelta::FromMilliseconds(2003),
                          TimeTicks() + TimeDelta::FromMilliseconds(2006),
                          TimeTicks() + TimeDelta::FromMilliseconds(2009),
                          TimeTicks() + TimeDelta::FromMilliseconds(3000),
                          TimeTicks() + TimeDelta::FromMilliseconds(3003)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       WakeUpBasedThrottling_DelayedTasks) {
  scheduler_->GetWakeUpBudgetPoolForTesting()->SetWakeUpDuration(
      TimeDelta::FromMilliseconds(10));
  std::vector<TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RunChainedTask, std::deque<TimeDelta>(10, TimeDelta()),
                     timer_queue_, test_task_runner_, &run_times,
                     TimeDelta::FromMilliseconds(3)),
      TimeDelta::FromMilliseconds(100));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(1000),
                          TimeTicks() + TimeDelta::FromMilliseconds(1003),
                          TimeTicks() + TimeDelta::FromMilliseconds(1006),
                          TimeTicks() + TimeDelta::FromMilliseconds(1009),
                          TimeTicks() + TimeDelta::FromMilliseconds(2000),
                          TimeTicks() + TimeDelta::FromMilliseconds(2003),
                          TimeTicks() + TimeDelta::FromMilliseconds(2006),
                          TimeTicks() + TimeDelta::FromMilliseconds(2009),
                          TimeTicks() + TimeDelta::FromMilliseconds(3000),
                          TimeTicks() + TimeDelta::FromMilliseconds(3003)));
}

TEST_F(TaskQueueThrottlerTest, WakeUpBasedThrottlingWithCPUBudgetThrottling) {
  scheduler_->GetWakeUpBudgetPoolForTesting()->SetWakeUpDuration(
      TimeDelta::FromMilliseconds(10));

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(TimeTicks(), 0.1);
  pool->AddQueue(TimeTicks(), timer_queue_.get());

  std::vector<TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &RunChainedTask,
          std::deque<TimeDelta>{
              TimeDelta::FromMilliseconds(250), TimeDelta(), TimeDelta(),
              TimeDelta::FromMilliseconds(250), TimeDelta(), TimeDelta(),
              TimeDelta::FromMilliseconds(250), TimeDelta(), TimeDelta()},
          timer_queue_, test_task_runner_, &run_times, TimeDelta()),
      TimeDelta::FromMilliseconds(100));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(TimeTicks() + TimeDelta::FromMilliseconds(1000),
                          TimeTicks() + TimeDelta::FromMilliseconds(3000),
                          TimeTicks() + TimeDelta::FromMilliseconds(3000),
                          TimeTicks() + TimeDelta::FromMilliseconds(3000),
                          TimeTicks() + TimeDelta::FromMilliseconds(6000),
                          TimeTicks() + TimeDelta::FromMilliseconds(6000),
                          TimeTicks() + TimeDelta::FromMilliseconds(6000),
                          TimeTicks() + TimeDelta::FromMilliseconds(8000),
                          TimeTicks() + TimeDelta::FromMilliseconds(8000)));
}

TEST_F(TaskQueueThrottlerTest,
       WakeUpBasedThrottlingWithCPUBudgetThrottling_OnAndOff) {
  scheduler_->GetWakeUpBudgetPoolForTesting()->SetWakeUpDuration(
      TimeDelta::FromMilliseconds(10));

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(TimeTicks(), 0.1);
  pool->AddQueue(TimeTicks(), timer_queue_.get());

  std::vector<TimeTicks> run_times;

  bool is_throttled = false;

  for (int i = 0; i < 5; ++i) {
    timer_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
        TimeDelta::FromMilliseconds(200));
    timer_task_runner_->PostDelayedTask(
        FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
        TimeDelta::FromMilliseconds(300));

    if (is_throttled) {
      task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
      is_throttled = false;
    } else {
      task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
      is_throttled = true;
    }

    test_task_runner_->FastForwardUntilNoTasksRemain();
  }

  EXPECT_THAT(run_times,
              ElementsAre(
                  // Throttled due to cpu budget.
                  TimeTicks() + TimeDelta::FromMilliseconds(1000),
                  TimeTicks() + TimeDelta::FromMilliseconds(3000),
                  // Unthrottled.
                  TimeTicks() + TimeDelta::FromMilliseconds(3200),
                  TimeTicks() + TimeDelta::FromMilliseconds(3450),
                  // Throttled due to wake-up budget. Old tasks still run.
                  TimeTicks() + TimeDelta::FromMilliseconds(5000),
                  TimeTicks() + TimeDelta::FromMilliseconds(5250),
                  // Unthrottled.
                  TimeTicks() + TimeDelta::FromMilliseconds(6200),
                  TimeTicks() + TimeDelta::FromMilliseconds(6450),
                  // Throttled due to wake-up budget. Old tasks still run.
                  TimeTicks() + TimeDelta::FromMilliseconds(8000),
                  TimeTicks() + TimeDelta::FromMilliseconds(8250)));
}

TEST_F(TaskQueueThrottlerTest,
       WakeUpBasedThrottlingWithCPUBudgetThrottling_ChainedFastTasks) {
  // This test checks that a new task should run during the wake-up window
  // when time budget allows that and should be blocked when time budget is
  // exhausted.
  scheduler_->GetWakeUpBudgetPoolForTesting()->SetWakeUpDuration(
      TimeDelta::FromMilliseconds(10));

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(TimeTicks(), 0.01);
  pool->AddQueue(TimeTicks(), timer_queue_.get());

  std::vector<TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RunChainedTask,
                     std::deque<TimeDelta>(10, TimeDelta::FromMilliseconds(7)),
                     timer_queue_, test_task_runner_, &run_times, TimeDelta()),
      TimeDelta::FromMilliseconds(100));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(
                  // Time budget is ~10ms and we can run two 7ms tasks.
                  TimeTicks() + TimeDelta::FromMilliseconds(1000),
                  TimeTicks() + TimeDelta::FromMilliseconds(1007),
                  // Time budget is ~6ms and we can run one 7ms task.
                  TimeTicks() + TimeDelta::FromMilliseconds(2000),
                  // Time budget is ~8ms and we can run two 7ms tasks.
                  TimeTicks() + TimeDelta::FromMilliseconds(3000),
                  TimeTicks() + TimeDelta::FromMilliseconds(3007),
                  // Time budget is ~5ms and we can run one 7ms task.
                  TimeTicks() + TimeDelta::FromMilliseconds(4000),
                  // Time budget is ~8ms and we can run two 7ms tasks.
                  TimeTicks() + TimeDelta::FromMilliseconds(5000),
                  TimeTicks() + TimeDelta::FromMilliseconds(5007),
                  // Time budget is ~4ms and we can run one 7ms task.
                  TimeTicks() + TimeDelta::FromMilliseconds(6000),
                  TimeTicks() + TimeDelta::FromMilliseconds(7000)));
}

}  // namespace task_queue_throttler_unittest
}  // namespace scheduler
}  // namespace blink
