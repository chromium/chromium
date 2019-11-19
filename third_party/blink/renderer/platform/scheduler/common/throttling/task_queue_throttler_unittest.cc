// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
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
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace task_queue_throttler_unittest {

using base::TestMockTimeTaskRunner;
using base::TimeDelta;
using base::TimeTicks;
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

Deque<base::TimeDelta> MakeTaskDurations(wtf_size_t size,
                                         base::TimeDelta duration) {
  Deque<base::TimeDelta> task_durations;
  for (wtf_size_t i = 0; i < size; ++i)
    task_durations.push_back(duration);
  return task_durations;
}

class TaskQueueThrottlerTest : public testing::Test {
 public:
  TaskQueueThrottlerTest()
      : test_task_runner_(base::MakeRefCounted<TestMockTimeTaskRunner>()) {}
  ~TaskQueueThrottlerTest() override = default;

  void SetUp() override {
    // A null clock triggers some assertions.
    test_task_runner_->AdvanceMockTickClock(
        base::TimeDelta::FromMilliseconds(5));

    scheduler_.reset(new MainThreadSchedulerImplForTest(
        base::sequence_manager::SequenceManagerForTest::Create(
            nullptr, test_task_runner_, GetTickClock()),
        base::nullopt));
    scheduler_->GetWakeUpBudgetPoolForTesting()->SetWakeUpDuration(
        base::TimeDelta());
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

    test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
    EXPECT_LE(count, 1u);

    // Make sure the rest of the tasks run or we risk a UAF on |count|.
    test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(10));
    EXPECT_EQ(10u, count);
  }

  void ExpectUnthrottled(scoped_refptr<TaskQueue> timer_queue) {
    size_t count = 0;
    timer_queue->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&RunTenTimesTask, &count, timer_queue));

    test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
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

  void SetAutoAdvanceInterval(base::TimeDelta interval) {
    advance_interval_ = interval;
  }

  base::TimeTicks NowTicks() const override {
    if (!advance_interval_.is_zero())
      task_runner_->AdvanceMockTickClock(advance_interval_);
    return task_runner_->NowTicks();
  }

 private:
  scoped_refptr<TestMockTimeTaskRunner> task_runner_;
  base::TimeDelta advance_interval_;
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
      proxy_clock_.SetAutoAdvanceInterval(
          base::TimeDelta::FromMicroseconds(10));
    }
  }

 protected:
  const base::TickClock* GetTickClock() const override { return &proxy_clock_; }

 private:
  AutoAdvancingProxyClock proxy_clock_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueThrottlerWithAutoAdvancingTimeTest);
};

INSTANTIATE_TEST_SUITE_P(,
                         TaskQueueThrottlerWithAutoAdvancingTimeTest,
                         testing::Bool());

TEST_F(TaskQueueThrottlerTest, ThrottledTasksReportRealTime) {
  EXPECT_EQ(timer_queue_->GetTimeDomain()->Now(),
            test_task_runner_->NowTicks());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_EQ(timer_queue_->GetTimeDomain()->Now(),
            test_task_runner_->NowTicks());

  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(250));
  // Make sure the throttled time domain's Now() reports the same as the
  // underlying clock.
  EXPECT_EQ(timer_queue_->GetTimeDomain()->Now(),
            test_task_runner_->NowTicks());
}

TEST_F(TaskQueueThrottlerTest, AlignedThrottledRunTime) {
  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.0)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.1)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.2)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.5)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.8)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.9)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(2.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(2.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(1.1)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(9.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(8.0)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(9.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(8.1)));
}

namespace {

// Round up time to milliseconds to deal with autoadvancing time.
// TODO(altimin): round time only when autoadvancing time is enabled.
base::TimeDelta RoundTimeToMilliseconds(base::TimeDelta time) {
  return time - time % base::TimeDelta::FromMilliseconds(1);
}

base::TimeTicks RoundTimeToMilliseconds(base::TimeTicks time) {
  return base::TimeTicks() + RoundTimeToMilliseconds(time - base::TimeTicks());
}

void TestTask(Vector<base::TimeTicks>* run_times,
              scoped_refptr<TestMockTimeTaskRunner> task_runner) {
  run_times->push_back(RoundTimeToMilliseconds(task_runner->NowTicks()));
  // FIXME No auto-advancing
}

void ExpensiveTestTask(Vector<base::TimeTicks>* run_times,
                       scoped_refptr<TestMockTimeTaskRunner> task_runner) {
  run_times->push_back(RoundTimeToMilliseconds(task_runner->NowTicks()));
  task_runner->AdvanceMockTickClock(base::TimeDelta::FromMilliseconds(250));
  // FIXME No auto-advancing
}

void RecordThrottling(Vector<base::TimeDelta>* reported_throttling_times,
                      base::TimeDelta throttling_duration) {
  reported_throttling_times->push_back(
      RoundTimeToMilliseconds(throttling_duration));
}
}  // namespace

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, TimerAlignment) {
  Vector<base::TimeTicks> run_times;
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200.0));

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(800.0));

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(1200.0));

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(8300.0));

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // Times are aligned to a multiple of 1000 milliseconds.
  EXPECT_THAT(
      run_times,
      ElementsAre(
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000.0),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000.0),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(2000.0),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(9000.0)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       TimerAlignment_Unthrottled) {
  Vector<base::TimeTicks> run_times;
  base::TimeTicks start_time = test_task_runner_->NowTicks();
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200.0));

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(800.0));

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(1200.0));

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(8300.0));

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // Times are not aligned.
  EXPECT_THAT(
      run_times,
      ElementsAre(RoundTimeToMilliseconds(
                      start_time + base::TimeDelta::FromMilliseconds(200.0)),
                  RoundTimeToMilliseconds(
                      start_time + base::TimeDelta::FromMilliseconds(800.0)),
                  RoundTimeToMilliseconds(
                      start_time + base::TimeDelta::FromMilliseconds(1200.0)),
                  RoundTimeToMilliseconds(
                      start_time + base::TimeDelta::FromMilliseconds(8300.0))));
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
                                                  base::TimeTicks());
  // Check PostPumpThrottledTasksLocked was called.
  EXPECT_FALSE(scheduler_->ControlTaskQueue()->IsEmpty());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       OnTimeDomainHasImmediateWork_DisabledQueue) {
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      timer_queue_->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  task_queue_throttler_->OnQueueNextWakeUpChanged(timer_queue_.get(),
                                                  base::TimeTicks());
  // Check PostPumpThrottledTasksLocked was not called.
  EXPECT_TRUE(scheduler_->ControlTaskQueue()->IsEmpty());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       ThrottlingADisabledQueueDoesNotPostPumpThrottledTasks) {
  timer_task_runner_->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      timer_queue_->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_TRUE(scheduler_->ControlTaskQueue()->IsEmpty());

  // Enabling it should trigger a call to PostPumpThrottledTasksLocked.
  voter->SetVoteToEnable(true);
  EXPECT_FALSE(scheduler_->ControlTaskQueue()->IsEmpty());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       ThrottlingADisabledQueueDoesNotPostPumpThrottledTasks_DelayedTask) {
  timer_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                                      base::TimeDelta::FromMilliseconds(1));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      timer_queue_->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_TRUE(scheduler_->ControlTaskQueue()->IsEmpty());

  // Enabling it should trigger a call to PostPumpThrottledTasksLocked.
  voter->SetVoteToEnable(true);
  EXPECT_FALSE(scheduler_->ControlTaskQueue()->IsEmpty());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, WakeUpForNonDelayedTask) {
  Vector<base::TimeTicks> run_times;

  // Nothing is posted on timer_queue_ so PumpThrottledTasks will not tick.
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Posting a task should trigger the pump.
  timer_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_));

  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() +
                          base::TimeDelta::FromMilliseconds(1000.0)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, WakeUpForDelayedTask) {
  Vector<base::TimeTicks> run_times;

  // Nothing is posted on timer_queue_ so PumpThrottledTasks will not tick.
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Posting a task should trigger the pump.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(1200.0));

  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() +
                          base::TimeDelta::FromMilliseconds(2000.0)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       SingleThrottledTaskPumpedAndRunWithNoExtraneousMessageLoopTasks) {
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  timer_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                                      delay);
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       SingleFutureThrottledTaskPumpedAndRunWithNoExtraneousMessageLoopTasks) {
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  base::TimeDelta delay(base::TimeDelta::FromSecondsD(15.5));
  timer_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                                      delay);
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       TwoFutureThrottledTaskPumpedAndRunWithNoExtraneousMessageLoopTasks) {
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  Vector<base::TimeTicks> run_times;

  base::TimeDelta delay(base::TimeDelta::FromSecondsD(15.5));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      delay);

  base::TimeDelta delay2(base::TimeDelta::FromSecondsD(5.5));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      delay2);

  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  test_task_runner_->FastForwardBy(test_task_runner_->NextPendingTaskDelay());
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  test_task_runner_->FastForwardBy(test_task_runner_->NextPendingTaskDelay());
  EXPECT_EQ(0u, test_task_runner_->GetPendingTaskCount());

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(6),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(16)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       TaskDelayIsBasedOnRealTime) {
  Vector<base::TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Post an initial task that should run at the first aligned time period.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(900.0));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // Advance realtime.
  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(250));

  // Post a task that due to real time + delay must run in the third aligned
  // time period.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(900.0));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(
      run_times,
      ElementsAre(
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000.0),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(3000.0)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, TaskQueueDisabledTillPump) {
  size_t count = 0;
  timer_task_runner_->PostTask(FROM_HERE, base::BindOnce(&AddOneTask, &count));

  EXPECT_FALSE(IsQueueBlocked(timer_queue_.get()));
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_TRUE(IsQueueBlocked(timer_queue_.get()));

  test_task_runner_->FastForwardUntilNoTasksRemain();  // Wait until the pump.
  EXPECT_EQ(1u, count);                                // The task got run.
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
  Vector<base::TimeTicks> run_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Submit two tasks. They should be aligned, and second one should be
  // throttled.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(3)));

  pool->RemoveQueue(test_task_runner_->NowTicks(), timer_queue_.get());
  run_times.clear();

  // Queue was removed from CPUTimeBudgetPool, only timer alignment should be
  // active now.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(4000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(4250)));

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  pool->Close();
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       EnableAndDisableCPUTimeBudgetPool) {
  Vector<base::TimeTicks> run_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");
  EXPECT_TRUE(pool->IsThrottlingEnabled());

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Post an expensive task. Pool is now throttled.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(1000)));
  run_times.clear();

  LazyNow lazy_now_1(test_task_runner_->GetMockTickClock());
  pool->DisableThrottling(&lazy_now_1);
  EXPECT_FALSE(pool->IsThrottlingEnabled());

  // Pool should not be throttled now.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(2000)));
  run_times.clear();

  LazyNow lazy_now_2(test_task_runner_->GetMockTickClock());
  pool->EnableThrottling(&lazy_now_2);
  EXPECT_TRUE(pool->IsThrottlingEnabled());

  // Because time pool was disabled, time budget level did not replenish
  // and queue is throttled.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(4000)));
  run_times.clear();

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());

  pool->RemoveQueue(test_task_runner_->NowTicks(), timer_queue_.get());
  pool->Close();
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       ImmediateTasksTimeBudgetThrottling) {
  Vector<base::TimeTicks> run_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

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

  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(3)));

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

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(4000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(4250)));

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  pool->Close();
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       TwoQueuesTimeBudgetThrottling) {
  Vector<base::TimeTicks> run_times;

  scoped_refptr<TaskQueue> second_queue = scheduler_->NewTimerTaskQueue(
      MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr);

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());
  pool->AddQueue(base::TimeTicks(), second_queue.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  task_queue_throttler_->IncreaseThrottleRefCount(second_queue.get());

  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));
  second_queue->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(3)));

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  task_queue_throttler_->DecreaseThrottleRefCount(second_queue.get());

  pool->RemoveQueue(test_task_runner_->NowTicks(), timer_queue_.get());
  pool->RemoveQueue(test_task_runner_->NowTicks(), second_queue.get());

  pool->Close();
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       DisabledTimeBudgetDoesNotAffectThrottledQueues) {
  Vector<base::TimeTicks> run_times;
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
      base::TimeDelta::FromMilliseconds(100));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(100));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1250)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       TimeBudgetThrottlingDoesNotAffectUnthrottledQueues) {
  Vector<base::TimeTicks> run_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");
  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);

  LazyNow lazy_now(test_task_runner_->GetMockTickClock());
  pool->DisableThrottling(&lazy_now);

  pool->AddQueue(test_task_runner_->NowTicks(), timer_queue_.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(100));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(100));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(105),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(355)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, MaxThrottlingDelay) {
  Vector<base::TimeTicks> run_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetMaxThrottlingDelay(base::TimeTicks(),
                              base::TimeDelta::FromMinutes(1));

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.001);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  for (int i = 0; i < 5; ++i) {
    timer_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
        base::TimeDelta::FromMilliseconds(200));
  }

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(62),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(123),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(184),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(245)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       EnableAndDisableThrottling) {
  Vector<base::TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(295));

  // Disable throttling - task should run immediately.
  task_queue_throttler_->DisableThrottling();

  test_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(200));

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(300)));
  run_times.clear();

  // Schedule a task at 900ms. It should proceed as normal.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(400));

  // Schedule a task at 1200ms. It should proceed as normal.
  // PumpThrottledTasks was scheduled at 1000ms, so it needs to be checked
  // that it was cancelled and it does not interfere with tasks posted before
  // 1s mark and scheduled to run after 1s mark.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(700));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(800));

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(900),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1200)));
  run_times.clear();

  // Schedule a task at 1500ms. It should be throttled because of enabled
  // throttling.
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(100));

  // Throttling is enabled and new task should be aligned.
  task_queue_throttler_->EnableThrottling();

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(2000)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, ReportThrottling) {
  Vector<base::TimeTicks> run_times;
  Vector<base::TimeDelta> reported_throttling_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

  pool->SetReportingCallback(
      base::BindRepeating(&RecordThrottling, &reported_throttling_times));

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(3)));

  EXPECT_THAT(reported_throttling_times,
              ElementsAre(base::TimeDelta::FromMilliseconds(1255),
                          base::TimeDelta::FromMilliseconds(1755)));

  pool->RemoveQueue(test_task_runner_->NowTicks(), timer_queue_.get());
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  pool->Close();
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, GrantAdditionalBudget) {
  Vector<base::TimeTicks> run_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());
  pool->GrantAdditionalBudget(base::TimeTicks(),
                              base::TimeDelta::FromMilliseconds(500));

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Submit five tasks. First three will not be throttled because they have
  // budget to run.
  for (int i = 0; i < 5; ++i) {
    timer_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
        base::TimeDelta::FromMilliseconds(200));
  }

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1250),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1500),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(3),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(6)));

  pool->RemoveQueue(test_task_runner_->NowTicks(), timer_queue_.get());
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  pool->Close();
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       EnableAndDisableThrottlingAndTimeBudgets) {
  // This test checks that if time budget pool is enabled when throttling
  // is disabled, it does not throttle the queue.
  Vector<base::TimeTicks> run_times;

  task_queue_throttler_->DisableThrottling();

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  LazyNow lazy_now_1(test_task_runner_->GetMockTickClock());
  pool->DisableThrottling(&lazy_now_1);

  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

  test_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(95));

  LazyNow lazy_now_2(test_task_runner_->GetMockTickClock());
  pool->EnableThrottling(&lazy_now_2);

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(300)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       AddQueueToBudgetPoolWhenThrottlingDisabled) {
  // This test checks that a task queue is added to time budget pool
  // when throttling is disabled, is does not throttle queue.
  Vector<base::TimeTicks> run_times;

  task_queue_throttler_->DisableThrottling();

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  test_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(95));

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));

  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(300)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       DisabledQueueThenEnabledQueue) {
  Vector<base::TimeTicks> run_times;

  scoped_refptr<MainThreadTaskQueue> second_queue =
      scheduler_->NewTimerTaskQueue(
          MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr);

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  task_queue_throttler_->IncreaseThrottleRefCount(second_queue.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(100));
  second_queue->task_runner()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      timer_queue_->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(250));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(1000)));

  voter->SetVoteToEnable(true);
  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(2000)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, TwoBudgetPools) {
  Vector<base::TimeTicks> run_times;

  scoped_refptr<TaskQueue> second_queue = scheduler_->NewTimerTaskQueue(
      MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr);

  CPUTimeBudgetPool* pool1 =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");
  pool1->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool1->AddQueue(base::TimeTicks(), timer_queue_.get());
  pool1->AddQueue(base::TimeTicks(), second_queue.get());

  CPUTimeBudgetPool* pool2 =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");
  pool2->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.01);
  pool2->AddQueue(base::TimeTicks(), timer_queue_.get());

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

  EXPECT_THAT(
      run_times,
      ElementsAre(
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(3000),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(6000),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(26000)));
}

namespace {

void RunChainedTask(Deque<base::TimeDelta> task_durations,
                    scoped_refptr<TaskQueue> queue,
                    scoped_refptr<TestMockTimeTaskRunner> task_runner,
                    Vector<base::TimeTicks>* run_times,
                    base::TimeDelta delay) {
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
      base::TimeDelta::FromMilliseconds(10));
  Vector<base::TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RunChainedTask, MakeTaskDurations(10, base::TimeDelta()),
                     timer_queue_, test_task_runner_, &run_times,
                     base::TimeDelta()),
      base::TimeDelta::FromMilliseconds(100));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(1)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       WakeUpBasedThrottling_ImmediateTasks_Fast) {
  scheduler_->GetWakeUpBudgetPoolForTesting()->SetWakeUpDuration(
      base::TimeDelta::FromMilliseconds(10));
  Vector<base::TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &RunChainedTask,
          MakeTaskDurations(10, base::TimeDelta::FromMilliseconds(3)),
          timer_queue_, test_task_runner_, &run_times, base::TimeDelta()),
      base::TimeDelta::FromMilliseconds(100));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // TODO(altimin): Add fence mechanism to block immediate tasks.
  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1003),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1006),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1009),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(2000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(2003),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(2006),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(2009),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(3000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(3003)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       WakeUpBasedThrottling_DelayedTasks) {
  scheduler_->GetWakeUpBudgetPoolForTesting()->SetWakeUpDuration(
      base::TimeDelta::FromMilliseconds(10));
  Vector<base::TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RunChainedTask, MakeTaskDurations(10, base::TimeDelta()),
                     timer_queue_, test_task_runner_, &run_times,
                     base::TimeDelta::FromMilliseconds(3)),
      base::TimeDelta::FromMilliseconds(100));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1003),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1006),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1009),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(2000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(2003),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(2006),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(2009),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(3000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(3003)));
}

TEST_F(TaskQueueThrottlerTest, WakeUpBasedThrottlingWithCPUBudgetThrottling) {
  scheduler_->GetWakeUpBudgetPoolForTesting()->SetWakeUpDuration(
      base::TimeDelta::FromMilliseconds(10));

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

  Vector<base::TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  Deque<base::TimeDelta> task_durations =
      MakeTaskDurations(9, base::TimeDelta());
  task_durations[0] = base::TimeDelta::FromMilliseconds(250);
  task_durations[3] = base::TimeDelta::FromMilliseconds(250);
  task_durations[6] = base::TimeDelta::FromMilliseconds(250);

  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RunChainedTask, std::move(task_durations), timer_queue_,
                     test_task_runner_, &run_times, base::TimeDelta()),
      base::TimeDelta::FromMilliseconds(100));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(3000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(3000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(3000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(6000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(6000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(6000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(8000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(8000)));
}

TEST_F(TaskQueueThrottlerTest,
       WakeUpBasedThrottlingWithCPUBudgetThrottling_OnAndOff) {
  scheduler_->GetWakeUpBudgetPoolForTesting()->SetWakeUpDuration(
      base::TimeDelta::FromMilliseconds(10));

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

  Vector<base::TimeTicks> run_times;

  bool is_throttled = false;

  for (int i = 0; i < 5; ++i) {
    timer_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_),
        base::TimeDelta::FromMilliseconds(200));
    timer_task_runner_->PostDelayedTask(
        FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
        base::TimeDelta::FromMilliseconds(300));

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
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(3000),
                  // Unthrottled.
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(3200),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(3450),
                  // Throttled due to wake-up budget. Old tasks still run.
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(5000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(5250),
                  // Unthrottled.
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(6200),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(6450),
                  // Throttled due to wake-up budget. Old tasks still run.
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(8000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(8250)));
}

TEST_F(TaskQueueThrottlerTest,
       WakeUpBasedThrottlingWithCPUBudgetThrottling_ChainedFastTasks) {
  // This test checks that a new task should run during the wake-up window
  // when time budget allows that and should be blocked when time budget is
  // exhausted.
  scheduler_->GetWakeUpBudgetPoolForTesting()->SetWakeUpDuration(
      base::TimeDelta::FromMilliseconds(10));

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.01);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

  Vector<base::TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &RunChainedTask,
          MakeTaskDurations(10, base::TimeDelta::FromMilliseconds(7)),
          timer_queue_, test_task_runner_, &run_times, base::TimeDelta()),
      base::TimeDelta::FromMilliseconds(100));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(
                  // Time budget is ~10ms and we can run two 7ms tasks.
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1007),
                  // Time budget is ~6ms and we can run one 7ms task.
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(2000),
                  // Time budget is ~8ms and we can run two 7ms tasks.
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(3000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(3007),
                  // Time budget is ~5ms and we can run one 7ms task.
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(4000),
                  // Time budget is ~8ms and we can run two 7ms tasks.
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(5000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(5007),
                  // Time budget is ~4ms and we can run one 7ms task.
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(6000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(7000)));
}

}  // namespace task_queue_throttler_unittest
}  // namespace scheduler
}  // namespace blink
