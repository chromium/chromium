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
#include "base/test/bind_test_util.h"
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
    timer_queue->task_runner()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RunTenTimesTask, count, timer_queue),
        base::TimeDelta::FromMilliseconds(1));
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
    task_queue_throttler_ = scheduler_->task_queue_throttler();
    wake_up_budget_pool_ =
        task_queue_throttler_->CreateWakeUpBudgetPool("Wake Up Budget Pool");
    wake_up_budget_pool_->SetWakeUpDuration(base::TimeDelta());
    timer_queue_ = scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kFrameThrottleable)
            .SetCanBeThrottled(true));
    wake_up_budget_pool_->AddQueue(base::TimeTicks(),
                                   timer_queue_->GetTaskQueue());
    timer_task_runner_ = timer_queue_->GetTaskQueue()->task_runner();
  }

  void TearDown() override {
    wake_up_budget_pool_->RemoveQueue(test_task_runner_->NowTicks(),
                                      timer_queue_->GetTaskQueue());
    wake_up_budget_pool_->Close();
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  void ExpectThrottled(scoped_refptr<TaskQueue> timer_queue) {
    size_t count = 0;
    timer_queue->task_runner()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RunTenTimesTask, &count, timer_queue),
        base::TimeDelta::FromMilliseconds(1));

    test_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(11));
    EXPECT_EQ(count, 0u);

    // Make sure the rest of the tasks run or we risk a UAF on |count|.
    test_task_runner_->FastForwardUntilNoTasksRemain();
    EXPECT_EQ(count, 10u);
  }

  void ExpectUnthrottled(scoped_refptr<TaskQueue> timer_queue) {
    size_t count = 0;
    timer_queue->task_runner()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RunTenTimesTask, &count, timer_queue),
        base::TimeDelta::FromMilliseconds(1));

    test_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(11));
    EXPECT_EQ(count, 10u);
  }

  bool IsQueueBlocked(TaskQueue* task_queue) {
    if (!task_queue->IsQueueEnabled())
      return true;
    return task_queue->BlockedByFence();
  }

  void ForwardTimeToNextMinute() {
    test_task_runner_->FastForwardBy(
        test_task_runner_->NowTicks().SnappedToNextTick(
            base::TimeTicks(), base::TimeDelta::FromMinutes(1)) -
        test_task_runner_->NowTicks());
  }

 protected:
  virtual const base::TickClock* GetTickClock() const {
    return test_task_runner_->GetMockTickClock();
  }

  scoped_refptr<TestMockTimeTaskRunner> test_task_runner_;
  std::unique_ptr<MainThreadSchedulerImplForTest> scheduler_;

  // A queue that is subject to |wake_up_budget_pool_|.
  scoped_refptr<MainThreadTaskQueue> timer_queue_;

  scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner_;
  TaskQueueThrottler* task_queue_throttler_ = nullptr;
  WakeUpBudgetPool* wake_up_budget_pool_ = nullptr;

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

INSTANTIATE_TEST_SUITE_P(All,
                         TaskQueueThrottlerWithAutoAdvancingTimeTest,
                         testing::Bool());

TEST_F(TaskQueueThrottlerTest, ThrottledTasksReportRealTime) {
  EXPECT_EQ(timer_queue_->GetTaskQueue()->GetTimeDomain()->Now(),
            test_task_runner_->NowTicks());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  EXPECT_EQ(timer_queue_->GetTaskQueue()->GetTimeDomain()->Now(),
            test_task_runner_->NowTicks());

  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(250));
  // Make sure the throttled time domain's Now() reports the same as the
  // underlying clock.
  EXPECT_EQ(timer_queue_->GetTaskQueue()->GetTimeDomain()->Now(),
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

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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
  ExpectUnthrottled(timer_queue_->GetTaskQueue());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  ExpectThrottled(timer_queue_->GetTaskQueue());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  ExpectThrottled(timer_queue_->GetTaskQueue());

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  ExpectThrottled(timer_queue_->GetTaskQueue());

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  ExpectUnthrottled(timer_queue_->GetTaskQueue());

  // Should be a NOP.
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  ExpectUnthrottled(timer_queue_->GetTaskQueue());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  ExpectThrottled(timer_queue_->GetTaskQueue());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       ThrotlingAnEmptyQueueDoesNotPostPumpThrottledTasksLocked) {
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

  EXPECT_TRUE(scheduler_->ControlTaskQueue()->GetTaskQueue()->IsEmpty());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       OnTimeDomainHasImmediateWork_EnabledQueue) {
  task_queue_throttler_->OnQueueNextWakeUpChanged(timer_queue_->GetTaskQueue(),
                                                  base::TimeTicks());
  // Check PostPumpThrottledTasksLocked was called.
  EXPECT_FALSE(scheduler_->ControlTaskQueue()->GetTaskQueue()->IsEmpty());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       OnTimeDomainHasImmediateWork_DisabledQueue) {
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      timer_queue_->GetTaskQueue()->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  task_queue_throttler_->OnQueueNextWakeUpChanged(timer_queue_->GetTaskQueue(),
                                                  base::TimeTicks());
  // Check PostPumpThrottledTasksLocked was not called.
  EXPECT_TRUE(scheduler_->ControlTaskQueue()->GetTaskQueue()->IsEmpty());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       ThrottlingADisabledQueueDoesNotPostPumpThrottledTasks) {
  timer_task_runner_->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      timer_queue_->GetTaskQueue()->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  EXPECT_TRUE(scheduler_->ControlTaskQueue()->GetTaskQueue()->IsEmpty());

  // Enabling it should trigger a call to PostPumpThrottledTasksLocked.
  voter->SetVoteToEnable(true);
  EXPECT_FALSE(scheduler_->ControlTaskQueue()->GetTaskQueue()->IsEmpty());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       ThrottlingADisabledQueueDoesNotPostPumpThrottledTasks_DelayedTask) {
  timer_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                                      base::TimeDelta::FromMilliseconds(1));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      timer_queue_->GetTaskQueue()->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  EXPECT_TRUE(scheduler_->ControlTaskQueue()->GetTaskQueue()->IsEmpty());

  // Enabling it should trigger a call to PostPumpThrottledTasksLocked.
  voter->SetVoteToEnable(true);
  EXPECT_FALSE(scheduler_->ControlTaskQueue()->GetTaskQueue()->IsEmpty());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, WakeUpForNonDelayedTask) {
  Vector<base::TimeTicks> run_times;

  // Nothing is posted on timer_queue_ so PumpThrottledTasks will not tick.
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  timer_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                                      delay);
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       SingleFutureThrottledTaskPumpedAndRunWithNoExtraneousMessageLoopTasks) {
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

  base::TimeDelta delay(base::TimeDelta::FromSecondsD(15.5));
  timer_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                                      delay);
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       TwoFutureThrottledTaskPumpedAndRunWithNoExtraneousMessageLoopTasks) {
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
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

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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

  EXPECT_FALSE(IsQueueBlocked(timer_queue_->GetTaskQueue()));
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  EXPECT_TRUE(IsQueueBlocked(timer_queue_->GetTaskQueue()));

  test_task_runner_->FastForwardUntilNoTasksRemain();  // Wait until the pump.
  EXPECT_EQ(1u, count);                                // The task got run.
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       DoubleIncrementDoubleDecrement) {
  timer_task_runner_->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  EXPECT_FALSE(IsQueueBlocked(timer_queue_->GetTaskQueue()));
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  EXPECT_TRUE(IsQueueBlocked(timer_queue_->GetTaskQueue()));
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  EXPECT_FALSE(IsQueueBlocked(timer_queue_->GetTaskQueue()));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       EnableVirtualTimeThenIncrement) {
  timer_task_runner_->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  scheduler_->EnableVirtualTime(
      MainThreadSchedulerImpl::BaseTimeOverridePolicy::DO_NOT_OVERRIDE);
  EXPECT_EQ(timer_queue_->GetTaskQueue()->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());

  EXPECT_FALSE(IsQueueBlocked(timer_queue_->GetTaskQueue()));
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  EXPECT_FALSE(IsQueueBlocked(timer_queue_->GetTaskQueue()));
  EXPECT_EQ(timer_queue_->GetTaskQueue()->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       IncrementThenEnableVirtualTime) {
  timer_task_runner_->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  EXPECT_FALSE(IsQueueBlocked(timer_queue_->GetTaskQueue()));
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  EXPECT_TRUE(IsQueueBlocked(timer_queue_->GetTaskQueue()));

  scheduler_->EnableVirtualTime(
      MainThreadSchedulerImpl::BaseTimeOverridePolicy::DO_NOT_OVERRIDE);
  EXPECT_FALSE(IsQueueBlocked(timer_queue_->GetTaskQueue()));
  EXPECT_EQ(timer_queue_->GetTaskQueue()->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, TimeBasedThrottling) {
  Vector<base::TimeTicks> run_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_->GetTaskQueue());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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

  pool->RemoveQueue(test_task_runner_->NowTicks(),
                    timer_queue_->GetTaskQueue());
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

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  pool->Close();
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       EnableAndDisableCPUTimeBudgetPool) {
  Vector<base::TimeTicks> run_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");
  EXPECT_TRUE(pool->IsThrottlingEnabled());

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_->GetTaskQueue());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_->GetTaskQueue());

  pool->RemoveQueue(test_task_runner_->NowTicks(),
                    timer_queue_->GetTaskQueue());
  pool->Close();
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       ImmediateTasksTimeBudgetThrottling) {
  Vector<base::TimeTicks> run_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_->GetTaskQueue());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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

  pool->RemoveQueue(test_task_runner_->NowTicks(),
                    timer_queue_->GetTaskQueue());
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

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  pool->Close();
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       TwoQueuesTimeBudgetThrottling) {
  Vector<base::TimeTicks> run_times;

  scoped_refptr<MainThreadTaskQueue> second_queue = scheduler_->NewTaskQueue(
      MainThreadTaskQueue::QueueCreationParams(
          MainThreadTaskQueue::QueueType::kFrameThrottleable)
          .SetCanBeThrottled(true));

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_->GetTaskQueue());
  pool->AddQueue(base::TimeTicks(), second_queue->GetTaskQueue());
  wake_up_budget_pool_->AddQueue(base::TimeTicks(),
                                 second_queue->GetTaskQueue());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  task_queue_throttler_->IncreaseThrottleRefCount(second_queue->GetTaskQueue());

  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));
  second_queue->GetTaskQueue()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(3)));

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  task_queue_throttler_->DecreaseThrottleRefCount(second_queue->GetTaskQueue());

  pool->RemoveQueue(test_task_runner_->NowTicks(),
                    timer_queue_->GetTaskQueue());
  pool->RemoveQueue(test_task_runner_->NowTicks(),
                    second_queue->GetTaskQueue());
  wake_up_budget_pool_->RemoveQueue(test_task_runner_->NowTicks(),
                                    second_queue->GetTaskQueue());

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

  pool->AddQueue(lazy_now.Now(), timer_queue_->GetTaskQueue());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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

  pool->AddQueue(test_task_runner_->NowTicks(), timer_queue_->GetTaskQueue());

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
  pool->AddQueue(base::TimeTicks(), timer_queue_->GetTaskQueue());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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
  pool->AddQueue(base::TimeTicks(), timer_queue_->GetTaskQueue());

  pool->SetReportingCallback(
      base::BindRepeating(&RecordThrottling, &reported_throttling_times));

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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

  pool->RemoveQueue(test_task_runner_->NowTicks(),
                    timer_queue_->GetTaskQueue());
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  pool->Close();
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, GrantAdditionalBudget) {
  Vector<base::TimeTicks> run_times;

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_->GetTaskQueue());
  pool->GrantAdditionalBudget(base::TimeTicks(),
                              base::TimeDelta::FromMilliseconds(500));

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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

  pool->RemoveQueue(test_task_runner_->NowTicks(),
                    timer_queue_->GetTaskQueue());
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_->GetTaskQueue());
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
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

  LazyNow lazy_now_1(test_task_runner_->GetMockTickClock());
  pool->DisableThrottling(&lazy_now_1);

  pool->AddQueue(base::TimeTicks(), timer_queue_->GetTaskQueue());

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
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

  test_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(95));

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));

  pool->AddQueue(base::TimeTicks(), timer_queue_->GetTaskQueue());

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(300)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       DisabledQueueThenEnabledQueue) {
  Vector<base::TimeTicks> run_times;

  scoped_refptr<MainThreadTaskQueue> second_queue = scheduler_->NewTaskQueue(
      MainThreadTaskQueue::QueueCreationParams(
          MainThreadTaskQueue::QueueType::kFrameThrottleable)
          .SetCanBeThrottled(true));

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  task_queue_throttler_->IncreaseThrottleRefCount(second_queue->GetTaskQueue());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(100));
  second_queue->GetTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      timer_queue_->GetTaskQueue()->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(250));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(1000)));

  // Advance time passed the 1-second aligned wake up. The next task will run on
  // the next 1-second aligned wake up.
  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(10));

  voter->SetVoteToEnable(true);
  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(2000)));
}

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest, TwoBudgetPools) {
  Vector<base::TimeTicks> run_times;

  scoped_refptr<MainThreadTaskQueue> second_queue = scheduler_->NewTaskQueue(
      MainThreadTaskQueue::QueueCreationParams(
          MainThreadTaskQueue::QueueType::kFrameThrottleable)
          .SetCanBeThrottled(true));

  wake_up_budget_pool_->AddQueue(base::TimeTicks(),
                                 second_queue->GetTaskQueue());

  CPUTimeBudgetPool* pool1 =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");
  pool1->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool1->AddQueue(base::TimeTicks(), timer_queue_->GetTaskQueue());
  pool1->AddQueue(base::TimeTicks(), second_queue->GetTaskQueue());

  CPUTimeBudgetPool* pool2 =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");
  pool2->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.01);
  pool2->AddQueue(base::TimeTicks(), timer_queue_->GetTaskQueue());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  task_queue_throttler_->IncreaseThrottleRefCount(second_queue->GetTaskQueue());

  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));
  second_queue->GetTaskQueue()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));
  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, &run_times, test_task_runner_));
  second_queue->GetTaskQueue()->task_runner()->PostTask(
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

  wake_up_budget_pool_->RemoveQueue(test_task_runner_->NowTicks(),
                                    second_queue->GetTaskQueue());
}

namespace {

void RunChainedTask(Deque<base::TimeDelta> task_durations,
                    scoped_refptr<MainThreadTaskQueue> queue,
                    scoped_refptr<TestMockTimeTaskRunner> task_runner,
                    Vector<base::TimeTicks>* run_times,
                    base::TimeDelta delay) {
  if (task_durations.empty())
    return;

  // FIXME No auto-advancing.

  run_times->push_back(RoundTimeToMilliseconds(task_runner->NowTicks()));
  task_runner->AdvanceMockTickClock(task_durations.front());
  task_durations.pop_front();

  queue->GetTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RunChainedTask, std::move(task_durations), queue,
                     task_runner, run_times, delay),
      delay);
}
}  // namespace

TEST_P(TaskQueueThrottlerWithAutoAdvancingTimeTest,
       WakeUpBasedThrottling_ChainedTasks_Instantaneous) {
  wake_up_budget_pool_->SetWakeUpDuration(
      base::TimeDelta::FromMilliseconds(10));
  Vector<base::TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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
  wake_up_budget_pool_->SetWakeUpDuration(
      base::TimeDelta::FromMilliseconds(10));
  Vector<base::TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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
  wake_up_budget_pool_->SetWakeUpDuration(
      base::TimeDelta::FromMilliseconds(10));
  Vector<base::TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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

TEST_F(TaskQueueThrottlerTest,
       WakeUpBasedThrottling_MultiplePoolsWithDifferentIntervalsOneEmpty) {
  // Have |wake_up_budget_pool_| control |task_queue| with a wake-up inteval
  // of one-minute.
  wake_up_budget_pool_->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                          base::TimeDelta::FromMinutes(1));
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  WakeUpBudgetPool* one_minute_pool = wake_up_budget_pool_;
  scoped_refptr<base::SingleThreadTaskRunner> one_minute_task_runner =
      timer_task_runner_;

  // Create another TaskQueue, throttled by another WakeUpBudgetPool with
  // a wake-up interval of two minutes.
  WakeUpBudgetPool* two_minutes_pool =
      task_queue_throttler_->CreateWakeUpBudgetPool(
          "Two Minutes Interval Pool");
  two_minutes_pool->SetWakeUpDuration(base::TimeDelta());
  two_minutes_pool->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                      base::TimeDelta::FromMinutes(2));
  scoped_refptr<MainThreadTaskQueue> two_minutes_queue =
      scheduler_->NewTaskQueue(
          MainThreadTaskQueue::QueueCreationParams(
              MainThreadTaskQueue::QueueType::kFrameThrottleable)
              .SetCanBeThrottled(true));
  two_minutes_pool->AddQueue(base::TimeTicks(),
                             two_minutes_queue->GetTaskQueue());
  scoped_refptr<base::SingleThreadTaskRunner> two_minutes_task_runner =
      two_minutes_queue->GetTaskQueue()->task_runner();
  task_queue_throttler_->IncreaseThrottleRefCount(
      two_minutes_queue->GetTaskQueue());

  // Post a task with a short delay to the first queue.
  constexpr base::TimeDelta kShortDelay = base::TimeDelta::FromSeconds(1);
  Vector<base::TimeTicks> run_times;
  one_minute_task_runner->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      kShortDelay);

  // Pools did not observe wake ups yet whether they had pending tasks or not.
  EXPECT_EQ(one_minute_pool->last_wake_up_for_testing(), base::nullopt);
  EXPECT_EQ(two_minutes_pool->last_wake_up_for_testing(), base::nullopt);

  // The first task should run after 1 minute, which is the wake up interval of
  // |one_minute_pool|.
  test_task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(one_minute_pool->last_wake_up_for_testing(),
            base::TimeTicks() + base::TimeDelta::FromMinutes(1));
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromMinutes(1)));

  // The second pool should not have woken up since it had no tasks.
  EXPECT_EQ(two_minutes_pool->last_wake_up_for_testing(), base::nullopt);

  // No new task execution or wake-ups for the first queue since it did not
  // get new tasks executed.
  test_task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(one_minute_pool->last_wake_up_for_testing(),
            base::TimeTicks() + base::TimeDelta::FromMinutes(1));
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromMinutes(1)));

  // The second pool should not have woken up since it had no tasks.
  EXPECT_EQ(two_minutes_pool->last_wake_up_for_testing(), base::nullopt);

  // Still no new executions so no update on the wake-up for the queues.
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(one_minute_pool->last_wake_up_for_testing(),
            base::TimeTicks() + base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(two_minutes_pool->last_wake_up_for_testing(), base::nullopt);

  // Clean up.
  two_minutes_pool->RemoveQueue(test_task_runner_->NowTicks(),
                                two_minutes_queue->GetTaskQueue());
  two_minutes_pool->Close();
}

TEST_F(TaskQueueThrottlerTest,
       WakeUpBasedThrottling_MultiplePoolsWithDifferentIntervals) {
  wake_up_budget_pool_->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                          base::TimeDelta::FromMinutes(1));
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  WakeUpBudgetPool* one_minute_pool = wake_up_budget_pool_;
  scoped_refptr<base::SingleThreadTaskRunner> one_minute_task_runner =
      timer_task_runner_;

  // Create another TaskQueue, throttled by another WakeUpBudgetPool.
  WakeUpBudgetPool* two_minutes_pool =
      task_queue_throttler_->CreateWakeUpBudgetPool(
          "Two Minutes Interval Pool");
  two_minutes_pool->SetWakeUpDuration(base::TimeDelta());
  two_minutes_pool->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                      base::TimeDelta::FromMinutes(2));
  scoped_refptr<MainThreadTaskQueue> two_minutes_queue =
      scheduler_->NewTaskQueue(
          MainThreadTaskQueue::QueueCreationParams(
              MainThreadTaskQueue::QueueType::kFrameThrottleable)
              .SetCanBeThrottled(true));
  two_minutes_pool->AddQueue(base::TimeTicks(),
                             two_minutes_queue->GetTaskQueue());
  scoped_refptr<base::SingleThreadTaskRunner> two_minutes_task_runner =
      two_minutes_queue->GetTaskQueue()->task_runner();
  task_queue_throttler_->IncreaseThrottleRefCount(
      two_minutes_queue->GetTaskQueue());

  // Post tasks with a short delay to both queues.
  constexpr base::TimeDelta kShortDelay = base::TimeDelta::FromSeconds(1);

  Vector<base::TimeTicks> run_times;
  one_minute_task_runner->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      kShortDelay);
  two_minutes_task_runner->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      kShortDelay);

  // Pools do not observe wake ups yet.
  EXPECT_EQ(one_minute_pool->last_wake_up_for_testing(), base::nullopt);
  EXPECT_EQ(two_minutes_pool->last_wake_up_for_testing(), base::nullopt);

  // The first task should run after 1 minute, which is the wake up interval of
  // |one_minute_pool|. The second task should run after 2 minutes, which is the
  // wake up interval of |two_minutes_pool|.
  test_task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(one_minute_pool->last_wake_up_for_testing(),
            base::TimeTicks() + base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(two_minutes_pool->last_wake_up_for_testing(), base::nullopt);
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromMinutes(1)));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(one_minute_pool->last_wake_up_for_testing(),
            base::TimeTicks() + base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(two_minutes_pool->last_wake_up_for_testing(),
            base::TimeTicks() + base::TimeDelta::FromMinutes(2));
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromMinutes(1),
                          base::TimeTicks() + base::TimeDelta::FromMinutes(2)));

  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(one_minute_pool->last_wake_up_for_testing(),
            base::TimeTicks() + base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(two_minutes_pool->last_wake_up_for_testing(),
            base::TimeTicks() + base::TimeDelta::FromMinutes(2));

  // Clean up.
  two_minutes_pool->RemoveQueue(test_task_runner_->NowTicks(),
                                two_minutes_queue->GetTaskQueue());
  two_minutes_pool->Close();
}

TEST_F(TaskQueueThrottlerTest,
       WakeUpBasedThrottling_MultiplePoolsWithUnalignedWakeUps) {
  // Snap the time to the next minute to simplify expectations.
  ForwardTimeToNextMinute();
  const base::TimeTicks start_time = test_task_runner_->NowTicks();

  wake_up_budget_pool_->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                          base::TimeDelta::FromMinutes(1));
  wake_up_budget_pool_->AllowUnalignedWakeUpIfNoRecentWakeUp();
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  WakeUpBudgetPool* one_minute_pool = wake_up_budget_pool_;
  scoped_refptr<base::SingleThreadTaskRunner> one_minute_task_runner =
      timer_task_runner_;

  // Create another TaskQueue, throttled by another WakeUpBudgetPool.
  WakeUpBudgetPool* two_minutes_pool =
      task_queue_throttler_->CreateWakeUpBudgetPool(
          "Two Minutes Interval Pool");
  two_minutes_pool->SetWakeUpDuration(base::TimeDelta());
  two_minutes_pool->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                      base::TimeDelta::FromMinutes(1));
  two_minutes_pool->AllowUnalignedWakeUpIfNoRecentWakeUp();
  scoped_refptr<MainThreadTaskQueue> two_minutes_queue =
      scheduler_->NewTaskQueue(
          MainThreadTaskQueue::QueueCreationParams(
              MainThreadTaskQueue::QueueType::kFrameThrottleable)
              .SetCanBeThrottled(true));
  two_minutes_pool->AddQueue(base::TimeTicks(),
                             two_minutes_queue->GetTaskQueue());
  scoped_refptr<base::SingleThreadTaskRunner> two_minutes_task_runner =
      two_minutes_queue->GetTaskQueue()->task_runner();
  task_queue_throttler_->IncreaseThrottleRefCount(
      two_minutes_queue->GetTaskQueue());

  // Post tasks with short delays to both queues. They should run unaligned. The
  // wake up in |one_minute_pool| should not be taken into account when
  // evaluating whether there was a recent wake up in
  // |two_minutes_pool_|.
  Vector<base::TimeTicks> run_times;
  one_minute_task_runner->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromSeconds(2));
  two_minutes_task_runner->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromSeconds(3));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_EQ(one_minute_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(2));
  EXPECT_EQ(two_minutes_pool->last_wake_up_for_testing(), base::nullopt);
  EXPECT_THAT(run_times,
              ElementsAre(start_time + base::TimeDelta::FromSeconds(2)));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(one_minute_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(2));
  EXPECT_EQ(two_minutes_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(3));
  EXPECT_THAT(run_times,
              ElementsAre(start_time + base::TimeDelta::FromSeconds(2),
                          start_time + base::TimeDelta::FromSeconds(3)));

  // Post extra tasks with long unaligned wake ups. They should run unaligned,
  // since their desired run time is more than 1 minute after the last wake up
  // in their respective pools.
  run_times.clear();
  one_minute_task_runner->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromSeconds(602));
  two_minutes_task_runner->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromSeconds(603));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(601));
  EXPECT_EQ(one_minute_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(2));
  EXPECT_EQ(two_minutes_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(3));
  EXPECT_THAT(run_times, ElementsAre());

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(one_minute_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(605));
  EXPECT_EQ(two_minutes_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(3));
  EXPECT_THAT(run_times,
              ElementsAre(start_time + base::TimeDelta::FromSeconds(605)));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(one_minute_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(605));
  EXPECT_EQ(two_minutes_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(606));
  EXPECT_THAT(run_times,
              ElementsAre(start_time + base::TimeDelta::FromSeconds(605),
                          start_time + base::TimeDelta::FromSeconds(606)));

  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(one_minute_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(605));
  EXPECT_EQ(two_minutes_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(606));

  // Clean up.
  two_minutes_pool->RemoveQueue(test_task_runner_->NowTicks(),
                                two_minutes_queue->GetTaskQueue());
  two_minutes_pool->Close();
}

TEST_F(TaskQueueThrottlerTest,
       WakeUpBasedThrottling_MultiplePoolsWithAllignedAndUnalignedWakeUps) {
  // Snap the time to the next minute to simplify expectations.
  ForwardTimeToNextMinute();
  const base::TimeTicks start_time = test_task_runner_->NowTicks();

  // The 1st WakeUpBudgetPool doesn't allow unaligned wake ups.
  wake_up_budget_pool_->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                          base::TimeDelta::FromMinutes(1));
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  WakeUpBudgetPool* aligned_pool = wake_up_budget_pool_;
  scoped_refptr<base::SingleThreadTaskRunner> aligned_task_runner =
      timer_task_runner_;

  // Create another TaskQueue, throttled by another WakeUpBudgetPool. This 2nd
  // WakeUpBudgetPool allows unaligned wake ups.
  WakeUpBudgetPool* unaligned_pool =
      task_queue_throttler_->CreateWakeUpBudgetPool(
          "Other Wake Up Budget Pool");
  unaligned_pool->SetWakeUpDuration(base::TimeDelta());
  unaligned_pool->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                    base::TimeDelta::FromMinutes(1));
  unaligned_pool->AllowUnalignedWakeUpIfNoRecentWakeUp();
  scoped_refptr<MainThreadTaskQueue> unaligned_queue = scheduler_->NewTaskQueue(
      MainThreadTaskQueue::QueueCreationParams(
          MainThreadTaskQueue::QueueType::kFrameThrottleable)
          .SetCanBeThrottled(true));
  unaligned_pool->AddQueue(base::TimeTicks(), unaligned_queue->GetTaskQueue());
  scoped_refptr<base::SingleThreadTaskRunner> unaligned_task_runner =
      unaligned_queue->GetTaskQueue()->task_runner();
  task_queue_throttler_->IncreaseThrottleRefCount(
      unaligned_queue->GetTaskQueue());

  // Post tasks with short delays to both queues. The 1st task should run
  // aligned, while the 2nd task should run unaligned.
  Vector<base::TimeTicks> run_times;
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromSeconds(2));
  unaligned_task_runner->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromSeconds(3));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_EQ(aligned_pool->last_wake_up_for_testing(), base::nullopt);
  EXPECT_EQ(unaligned_pool->last_wake_up_for_testing(), base::nullopt);
  EXPECT_THAT(run_times, ElementsAre());

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(aligned_pool->last_wake_up_for_testing(), base::nullopt);
  EXPECT_EQ(unaligned_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(3));
  EXPECT_THAT(run_times,
              ElementsAre(start_time + base::TimeDelta::FromSeconds(3)));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(57));
  EXPECT_EQ(aligned_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(unaligned_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(3));
  EXPECT_THAT(run_times,
              ElementsAre(start_time + base::TimeDelta::FromSeconds(3),
                          start_time + base::TimeDelta::FromMinutes(1)));

  // Post extra tasks with long unaligned wake ups. The 1st task should run
  // aligned, while the 2nd task should run unaligned.
  run_times.clear();
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromSeconds(602));
  unaligned_task_runner->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromSeconds(603));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(601));
  EXPECT_EQ(aligned_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(unaligned_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(3));
  EXPECT_THAT(run_times, ElementsAre());

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_EQ(aligned_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(unaligned_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(663));
  EXPECT_THAT(run_times,
              ElementsAre(start_time + base::TimeDelta::FromSeconds(663)));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(117));
  EXPECT_EQ(aligned_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromMinutes(12));
  EXPECT_EQ(unaligned_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(663));
  EXPECT_THAT(run_times,
              ElementsAre(start_time + base::TimeDelta::FromSeconds(663),
                          start_time + base::TimeDelta::FromMinutes(12)));

  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(aligned_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromMinutes(12));
  EXPECT_EQ(unaligned_pool->last_wake_up_for_testing(),
            start_time + base::TimeDelta::FromSeconds(663));

  // Clean up.
  unaligned_pool->RemoveQueue(test_task_runner_->NowTicks(),
                              unaligned_queue->GetTaskQueue());
  unaligned_pool->Close();
}

TEST_F(TaskQueueThrottlerTest, WakeUpBasedThrottling_EnableDisableThrottling) {
  constexpr base::TimeDelta kDelay = base::TimeDelta::FromSeconds(10);
  constexpr base::TimeDelta kTimeBetweenWakeUps =
      base::TimeDelta::FromMinutes(1);
  wake_up_budget_pool_->SetWakeUpInterval(base::TimeTicks(),
                                          kTimeBetweenWakeUps);
  wake_up_budget_pool_->SetWakeUpDuration(base::TimeDelta::FromMilliseconds(1));
  Vector<base::TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RunChainedTask, MakeTaskDurations(10, base::TimeDelta()),
                     timer_queue_, test_task_runner_, &run_times, kDelay),
      kDelay);

  // Throttling is enabled. Only 1 task runs per |kTimeBetweenWakeUps|.
  test_task_runner_->FastForwardBy(kTimeBetweenWakeUps);

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromSeconds(60)));
  run_times.clear();

  // Disable throttling. All tasks can run.
  LazyNow lazy_now_1(test_task_runner_->GetMockTickClock());
  wake_up_budget_pool_->DisableThrottling(&lazy_now_1);
  test_task_runner_->FastForwardBy(5 * kDelay);

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(70),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(80),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(90),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(100),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(110)));
  run_times.clear();

  // Throttling is enabled. Only 1 task runs per |kTimeBetweenWakeUps|.
  LazyNow lazy_now_2(test_task_runner_->GetMockTickClock());
  wake_up_budget_pool_->EnableThrottling(&lazy_now_2);
  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(120),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(180),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(240),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(300)));
}

TEST_F(TaskQueueThrottlerTest, WakeUpBasedThrottling_UnalignedWakeUps) {
  // All throttled wake ups are aligned on 1-second intervals by
  // TaskQueueThrottler, irrespective of BudgetPools. Start the test at a time
  // aligned on a 1-minute interval, to simplify expectations.
  ForwardTimeToNextMinute();
  const base::TimeTicks start_time = test_task_runner_->NowTicks();

  Vector<base::TimeTicks> run_times;
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

  wake_up_budget_pool_->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                          base::TimeDelta::FromMinutes(1));
  wake_up_budget_pool_->AllowUnalignedWakeUpIfNoRecentWakeUp();

  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromSeconds(90));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(start_time + base::TimeDelta::FromSeconds(90)));
}

TEST_F(TaskQueueThrottlerTest,
       WakeUpBasedThrottling_UnalignedWakeUps_MultipleTasks) {
  // Start at a 1-minute aligned time to simplify expectations.
  ForwardTimeToNextMinute();
  const base::TimeTicks initial_time = test_task_runner_->NowTicks();
  wake_up_budget_pool_->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                          base::TimeDelta::FromMinutes(1));
  wake_up_budget_pool_->AllowUnalignedWakeUpIfNoRecentWakeUp();
  Vector<base::TimeTicks> run_times;
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());
  // Task delay:        Expected run time:    Reason:
  // 30 seconds         30 seconds            >= 60 seconds after last wake up
  // 80 seconds         90 seconds            >= 60 seconds after last wake up
  // 95 seconds         120 seconds           Aligned
  // 100 seconds        120 seconds           Aligned
  // 130 seconds        180 seconds           Aligned
  // 251 seconds        251 seconds           >= 60 seconds after last wake up
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromSeconds(30));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromSeconds(80));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromSeconds(95));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromSeconds(100));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromSeconds(130));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromSeconds(251));
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(run_times,
              ElementsAre(initial_time + base::TimeDelta::FromSeconds(30),
                          initial_time + base::TimeDelta::FromSeconds(90),
                          initial_time + base::TimeDelta::FromSeconds(120),
                          initial_time + base::TimeDelta::FromSeconds(120),
                          initial_time + base::TimeDelta::FromSeconds(180),
                          initial_time + base::TimeDelta::FromSeconds(251)));
}

TEST_F(TaskQueueThrottlerTest,
       WakeUpBasedThrottling_IncreaseWakeUpIntervalBeforeWakeUp) {
  Vector<base::TimeTicks> run_times;
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

  // Post 2 delayed tasks when the wake up interval is 1 minute. The delay of
  // the 2nd task is such that it won't be ready when the 1st task completes.
  wake_up_budget_pool_->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                          base::TimeDelta::FromMinutes(1));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(1));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMinutes(2));

  // Update the wake up interval to 1 hour.
  wake_up_budget_pool_->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                          base::TimeDelta::FromHours(1));

  // Tasks run after 1 hour, which is the most up to date wake up interval.
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromHours(1),
                          base::TimeTicks() + base::TimeDelta::FromHours(1)));
}

TEST_F(TaskQueueThrottlerTest,
       WakeUpBasedThrottling_DecreaseWakeUpIntervalBeforeWakeUp) {
  Vector<base::TimeTicks> run_times;
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

  // Post a delayed task when the wake up interval is 1 hour.
  wake_up_budget_pool_->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                          base::TimeDelta::FromHours(1));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(1));

  // Update the wake up interval to 1 minute.
  wake_up_budget_pool_->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                          base::TimeDelta::FromMinutes(1));

  // The delayed task should run after 1 minute, which is the most up to date
  // wake up interval.
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromMinutes(1)));
}

TEST_F(TaskQueueThrottlerTest,
       WakeUpBasedThrottling_IncreaseWakeUpIntervalDuringWakeUp) {
  wake_up_budget_pool_->SetWakeUpDuration(
      base::TimeDelta::FromMilliseconds(10));

  Vector<base::TimeTicks> run_times;
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

  // Post a 1st delayed task when the wake up interval is 1 minute.
  wake_up_budget_pool_->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                          base::TimeDelta::FromMinutes(1));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        TestTask(&run_times, test_task_runner_);
        // Post a 2nd delayed task when the wake up interval is still 1 minute.
        timer_task_runner_->PostDelayedTask(
            FROM_HERE, base::BindLambdaForTesting([&]() {
              TestTask(&run_times, test_task_runner_);
              // Post a 3rd task when the wake up interval is 1 hour.
              timer_task_runner_->PostDelayedTask(
                  FROM_HERE,
                  base::BindOnce(&TestTask, &run_times, test_task_runner_),
                  base::TimeDelta::FromSeconds(1));
            }),
            base::TimeDelta::FromSeconds(1));
        // Increase the wake up interval. This should affect the 2nd and 3rd
        // tasks, which haven't run yet.
        wake_up_budget_pool_->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                                base::TimeDelta::FromHours(1));
      }),
      base::TimeDelta::FromSeconds(1));

  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromMinutes(1),
                          base::TimeTicks() + base::TimeDelta::FromHours(1),
                          base::TimeTicks() + base::TimeDelta::FromHours(2)));
}

TEST_F(TaskQueueThrottlerTest,
       WakeUpBasedThrottling_DecreaseWakeUpIntervalDuringWakeUp) {
  wake_up_budget_pool_->SetWakeUpDuration(
      base::TimeDelta::FromMilliseconds(10));

  Vector<base::TimeTicks> run_times;
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

  // Post a 1st delayed task when the wake up interval is 1 hour.
  wake_up_budget_pool_->SetWakeUpInterval(test_task_runner_->NowTicks(),
                                          base::TimeDelta::FromHours(1));
  timer_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        TestTask(&run_times, test_task_runner_);
        // Post a 2nd delayed task when the wake up interval is still 1 hour.
        timer_task_runner_->PostDelayedTask(
            FROM_HERE, base::BindLambdaForTesting([&]() {
              TestTask(&run_times, test_task_runner_);
              // Post a 3rd task when the wake up interval is 1 minute.
              timer_task_runner_->PostDelayedTask(
                  FROM_HERE,
                  base::BindOnce(&TestTask, &run_times, test_task_runner_),
                  base::TimeDelta::FromSeconds(1));
            }),
            base::TimeDelta::FromSeconds(1));
        // Decrease the wake up interval. This immediately reschedules the wake
        // up for the 2nd task.
        wake_up_budget_pool_->SetWakeUpInterval(
            test_task_runner_->NowTicks(), base::TimeDelta::FromMinutes(1));
      }),
      base::TimeDelta::FromSeconds(1));

  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromHours(1),
                          base::TimeTicks() + base::TimeDelta::FromHours(1) +
                              base::TimeDelta::FromMinutes(1),
                          base::TimeTicks() + base::TimeDelta::FromHours(1) +
                              base::TimeDelta::FromMinutes(2)));
}

TEST_F(TaskQueueThrottlerTest, WakeUpBasedThrottlingWithCPUBudgetThrottling) {
  wake_up_budget_pool_->SetWakeUpDuration(
      base::TimeDelta::FromMilliseconds(10));

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_->GetTaskQueue());

  Vector<base::TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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
  wake_up_budget_pool_->SetWakeUpDuration(
      base::TimeDelta::FromMilliseconds(10));

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_->GetTaskQueue());

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
      task_queue_throttler_->DecreaseThrottleRefCount(
          timer_queue_->GetTaskQueue());
      is_throttled = false;
    } else {
      task_queue_throttler_->IncreaseThrottleRefCount(
          timer_queue_->GetTaskQueue());
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
  wake_up_budget_pool_->SetWakeUpDuration(
      base::TimeDelta::FromMilliseconds(10));

  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.01);
  pool->AddQueue(base::TimeTicks(), timer_queue_->GetTaskQueue());

  Vector<base::TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_->GetTaskQueue());

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
