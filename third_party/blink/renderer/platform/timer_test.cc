// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/timer.h"

#include <memory>
#include <queue>
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

using base::sequence_manager::TaskQueue;
using blink::scheduler::MainThreadTaskQueue;
using testing::ElementsAre;

namespace blink {
namespace {

class TimerTest : public testing::Test {
 public:
  TimerTest() {
    scoped_refptr<MainThreadTaskQueue> task_queue =
        platform_->GetMainThreadScheduler()->NewTaskQueue(
            MainThreadTaskQueue::QueueCreationParams(
                MainThreadTaskQueue::QueueType::kTest));
    task_runner_ = task_queue->CreateTaskRunner(TaskType::kInternalTest);
  }

  void SetUp() override {
    run_times_.clear();
    platform_->AdvanceClock(TimeDelta::FromSeconds(10));
    start_time_ = CurrentTimeTicks();
  }

  void CountingTask(TimerBase*) { run_times_.push_back(CurrentTimeTicks()); }

  void RecordNextFireTimeTask(TimerBase* timer) {
    next_fire_times_.push_back(CurrentTimeTicks() + timer->NextFireInterval());
  }

  void RunUntilDeadline(TimeTicks deadline) {
    TimeDelta period = deadline - CurrentTimeTicks();
    EXPECT_GE(period, TimeDelta());
    platform_->RunForPeriod(period);
  }

  // Returns false if there are no pending delayed tasks, otherwise sets |time|
  // to the delay in seconds till the next pending delayed task is scheduled to
  // fire.
  bool TimeTillNextDelayedTask(TimeDelta* time) const {
    base::sequence_manager::LazyNow lazy_now =
        platform_->GetMainThreadScheduler()
            ->real_time_domain()
            ->CreateLazyNow();
    base::Optional<base::TimeDelta> delay = platform_->GetMainThreadScheduler()
                                                ->GetActiveTimeDomain()
                                                ->DelayTillNextTask(&lazy_now);
    if (!delay)
      return false;
    *time = *delay;
    return true;
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
    return task_runner_;
  }

 protected:
  TimeTicks start_time_;
  WTF::Vector<TimeTicks> run_times_;
  WTF::Vector<TimeTicks> next_fire_times_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::MessageLoop message_loop_;
};

class OnHeapTimerOwner final
    : public GarbageCollectedFinalized<OnHeapTimerOwner> {
 public:
  class Record final : public RefCounted<Record> {
   public:
    static scoped_refptr<Record> Create() { return base::AdoptRef(new Record); }

    bool TimerHasFired() const { return timer_has_fired_; }
    bool IsDisposed() const { return is_disposed_; }
    bool OwnerIsDestructed() const { return owner_is_destructed_; }
    void SetTimerHasFired() { timer_has_fired_ = true; }
    void Dispose() { is_disposed_ = true; }
    void SetOwnerIsDestructed() { owner_is_destructed_ = true; }

   private:
    Record() = default;

    bool timer_has_fired_ = false;
    bool is_disposed_ = false;
    bool owner_is_destructed_ = false;
  };

  explicit OnHeapTimerOwner(
      scoped_refptr<Record> record,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : timer_(std::move(task_runner), this, &OnHeapTimerOwner::Fired),
        record_(std::move(record)) {}
  ~OnHeapTimerOwner() { record_->SetOwnerIsDestructed(); }

  void StartOneShot(TimeDelta interval, const base::Location& caller) {
    timer_.StartOneShot(interval, caller);
  }

  void Trace(blink::Visitor* visitor) {}

 private:
  void Fired(TimerBase*) {
    EXPECT_FALSE(record_->IsDisposed());
    record_->SetTimerHasFired();
  }

  TaskRunnerTimer<OnHeapTimerOwner> timer_;
  scoped_refptr<Record> record_;
};

class GCForbiddenScope final {
  STACK_ALLOCATED();

 public:
  GCForbiddenScope() { ThreadState::Current()->EnterGCForbiddenScope(); }
  ~GCForbiddenScope() { ThreadState::Current()->LeaveGCForbiddenScope(); }
};

TEST_F(TimerTest, StartOneShot_Zero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta(), FROM_HERE);

  TimeDelta run_time;
  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_));
}

TEST_F(TimerTest, StartOneShot_ZeroAndCancel) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta(), FROM_HERE);

  TimeDelta run_time;
  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  timer.Stop();

  platform_->RunUntilIdle();
  EXPECT_FALSE(run_times_.size());
}

TEST_F(TimerTest, StartOneShot_ZeroAndCancelThenRepost) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta(), FROM_HERE);

  TimeDelta run_time;
  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  timer.Stop();

  platform_->RunUntilIdle();
  EXPECT_FALSE(run_times_.size());

  timer.StartOneShot(TimeDelta(), FROM_HERE);

  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_));
}

TEST_F(TimerTest, StartOneShot_Zero_RepostingAfterRunning) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta(), FROM_HERE);

  TimeDelta run_time;
  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_));

  timer.StartOneShot(TimeDelta(), FROM_HERE);

  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_, start_time_));
}

TEST_F(TimerTest, StartOneShot_NonZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta::FromSeconds(10), FROM_HERE);

  TimeDelta run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(TimeDelta::FromSeconds(10), run_time);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_,
              ElementsAre(start_time_ + TimeDelta::FromSeconds(10)));
}

TEST_F(TimerTest, StartOneShot_NonZeroAndCancel) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta::FromSeconds(10), FROM_HERE);

  TimeDelta run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(TimeDelta::FromSeconds(10), run_time);

  timer.Stop();
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_FALSE(run_times_.size());
}

TEST_F(TimerTest, StartOneShot_NonZeroAndCancelThenRepost) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta::FromSeconds(10), FROM_HERE);

  TimeDelta run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(TimeDelta::FromSeconds(10), run_time);

  timer.Stop();
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_FALSE(run_times_.size());

  TimeTicks second_post_time = CurrentTimeTicks();
  timer.StartOneShot(TimeDelta::FromSeconds(10), FROM_HERE);

  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(TimeDelta::FromSeconds(10), run_time);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_,
              ElementsAre(second_post_time + TimeDelta::FromSeconds(10)));
}

TEST_F(TimerTest, StartOneShot_NonZero_RepostingAfterRunning) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta::FromSeconds(10), FROM_HERE);

  TimeDelta run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(TimeDelta::FromSeconds(10), run_time);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_,
              ElementsAre(start_time_ + TimeDelta::FromSeconds(10)));

  timer.StartOneShot(TimeDelta::FromSeconds(20), FROM_HERE);

  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(TimeDelta::FromSeconds(20), run_time);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_,
              ElementsAre(start_time_ + TimeDelta::FromSeconds(10),
                          start_time_ + TimeDelta::FromSeconds(30)));
}

TEST_F(TimerTest, PostingTimerTwiceWithSameRunTimeDoesNothing) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta::FromSeconds(10), FROM_HERE);
  timer.StartOneShot(TimeDelta::FromSeconds(10), FROM_HERE);

  TimeDelta run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(TimeDelta::FromSeconds(10), run_time);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_,
              ElementsAre(start_time_ + TimeDelta::FromSeconds(10)));
}

TEST_F(TimerTest, PostingTimerTwiceWithNewerRunTimeCancelsOriginalTask) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta::FromSeconds(10), FROM_HERE);
  timer.StartOneShot(TimeDelta(), FROM_HERE);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + TimeDelta::FromSeconds(0)));
}

TEST_F(TimerTest, PostingTimerTwiceWithLaterRunTimeCancelsOriginalTask) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta(), FROM_HERE);
  timer.StartOneShot(TimeDelta::FromSeconds(10), FROM_HERE);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_,
              ElementsAre(start_time_ + TimeDelta::FromSeconds(10)));
}

TEST_F(TimerTest, StartRepeatingTask) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(TimeDelta::FromSeconds(1), FROM_HERE);

  TimeDelta run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(TimeDelta::FromSeconds(1), run_time);

  RunUntilDeadline(start_time_ + TimeDelta::FromMilliseconds(5500));
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + TimeDelta::FromSeconds(1),
                                      start_time_ + TimeDelta::FromSeconds(2),
                                      start_time_ + TimeDelta::FromSeconds(3),
                                      start_time_ + TimeDelta::FromSeconds(4),
                                      start_time_ + TimeDelta::FromSeconds(5)));
}

TEST_F(TimerTest, StartRepeatingTask_ThenCancel) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(TimeDelta::FromSeconds(1), FROM_HERE);

  TimeDelta run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(TimeDelta::FromSeconds(1), run_time);

  RunUntilDeadline(start_time_ + TimeDelta::FromMilliseconds(2500));
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + TimeDelta::FromSeconds(1),
                                      start_time_ + TimeDelta::FromSeconds(2)));

  timer.Stop();
  platform_->RunUntilIdle();

  EXPECT_THAT(run_times_, ElementsAre(start_time_ + TimeDelta::FromSeconds(1),
                                      start_time_ + TimeDelta::FromSeconds(2)));
}

TEST_F(TimerTest, StartRepeatingTask_ThenPostOneShot) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(TimeDelta::FromSeconds(1), FROM_HERE);

  TimeDelta run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(TimeDelta::FromSeconds(1), run_time);

  RunUntilDeadline(start_time_ + TimeDelta::FromMilliseconds(2500));
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + TimeDelta::FromSeconds(1),
                                      start_time_ + TimeDelta::FromSeconds(2)));

  timer.StartOneShot(TimeDelta(), FROM_HERE);
  platform_->RunUntilIdle();

  EXPECT_THAT(run_times_,
              ElementsAre(start_time_ + TimeDelta::FromSeconds(1),
                          start_time_ + TimeDelta::FromSeconds(2),
                          start_time_ + TimeDelta::FromMilliseconds(2500)));
}

TEST_F(TimerTest, IsActive_NeverPosted) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);

  EXPECT_FALSE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_OneShotZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta(), FROM_HERE);

  EXPECT_TRUE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_OneShotNonZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta::FromSeconds(10), FROM_HERE);

  EXPECT_TRUE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_Repeating) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(TimeDelta::FromSeconds(1), FROM_HERE);

  EXPECT_TRUE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_OneShotZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta(), FROM_HERE);

  platform_->RunUntilIdle();
  EXPECT_FALSE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_OneShotNonZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta::FromSeconds(10), FROM_HERE);

  platform_->RunUntilIdle();
  EXPECT_FALSE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_Repeating) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(TimeDelta::FromSeconds(1), FROM_HERE);

  RunUntilDeadline(start_time_ + TimeDelta::FromSeconds(10));
  EXPECT_TRUE(timer.IsActive());  // It should run until cancelled.
}

TEST_F(TimerTest, NextFireInterval_OneShotZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta(), FROM_HERE);

  EXPECT_TRUE(timer.NextFireInterval().is_zero());
}

TEST_F(TimerTest, NextFireInterval_OneShotNonZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta::FromSeconds(10), FROM_HERE);

  EXPECT_EQ(TimeDelta::FromSeconds(10), timer.NextFireInterval());
}

TEST_F(TimerTest, NextFireInterval_OneShotNonZero_AfterAFewSeconds) {
  platform_->SetAutoAdvanceNowToPendingTasks(false);

  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta::FromSeconds(10), FROM_HERE);

  platform_->AdvanceClock(TimeDelta::FromSeconds(2));
  EXPECT_EQ(TimeDelta::FromSeconds(8), timer.NextFireInterval());
}

TEST_F(TimerTest, NextFireInterval_Repeating) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(TimeDelta::FromSeconds(20), FROM_HERE);

  EXPECT_EQ(TimeDelta::FromSeconds(20), timer.NextFireInterval());
}

TEST_F(TimerTest, RepeatInterval_NeverStarted) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);

  EXPECT_TRUE(timer.RepeatInterval().is_zero());
}

TEST_F(TimerTest, RepeatInterval_OneShotZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta(), FROM_HERE);

  EXPECT_TRUE(timer.RepeatInterval().is_zero());
}

TEST_F(TimerTest, RepeatInterval_OneShotNonZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta::FromSeconds(10), FROM_HERE);

  EXPECT_TRUE(timer.RepeatInterval().is_zero());
}

TEST_F(TimerTest, RepeatInterval_Repeating) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(TimeDelta::FromSeconds(20), FROM_HERE);

  EXPECT_EQ(TimeDelta::FromSeconds(20), timer.RepeatInterval());
}

TEST_F(TimerTest, AugmentRepeatInterval) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(TimeDelta::FromSeconds(10), FROM_HERE);
  EXPECT_EQ(TimeDelta::FromSeconds(10), timer.RepeatInterval());
  EXPECT_EQ(TimeDelta::FromSeconds(10), timer.NextFireInterval());

  platform_->AdvanceClock(TimeDelta::FromSeconds(2));
  timer.AugmentRepeatInterval(TimeDelta::FromSeconds(10));

  EXPECT_EQ(TimeDelta::FromSeconds(20), timer.RepeatInterval());
  EXPECT_EQ(TimeDelta::FromSeconds(18), timer.NextFireInterval());

  RunUntilDeadline(start_time_ + TimeDelta::FromSeconds(50));
  EXPECT_THAT(run_times_,
              ElementsAre(start_time_ + TimeDelta::FromSeconds(20),
                          start_time_ + TimeDelta::FromSeconds(40)));
}

TEST_F(TimerTest, AugmentRepeatInterval_TimerFireDelayed) {
  platform_->SetAutoAdvanceNowToPendingTasks(false);

  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(TimeDelta::FromSeconds(10), FROM_HERE);
  EXPECT_EQ(TimeDelta::FromSeconds(10), timer.RepeatInterval());
  EXPECT_EQ(TimeDelta::FromSeconds(10), timer.NextFireInterval());

  platform_->AdvanceClock(
      TimeDelta::FromSeconds(123));  // Make the timer long overdue.
  timer.AugmentRepeatInterval(TimeDelta::FromSeconds(10));

  EXPECT_EQ(TimeDelta::FromSeconds(20), timer.RepeatInterval());
  // The timer is overdue so it should be scheduled to fire immediatly.
  EXPECT_TRUE(timer.NextFireInterval().is_zero());
}

TEST_F(TimerTest, RepeatingTimerDoesNotDrift) {
  platform_->SetAutoAdvanceNowToPendingTasks(false);

  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::RecordNextFireTimeTask);
  timer.StartRepeating(TimeDelta::FromSeconds(2), FROM_HERE);

  RecordNextFireTimeTask(
      &timer);  // Next scheduled task to run at |start_time_| + 2s

  // Simulate timer firing early. Next scheduled task to run at
  // |start_time_| + 4s
  platform_->AdvanceClock(TimeDelta::FromMilliseconds(1900));
  RunUntilDeadline(CurrentTimeTicks() + TimeDelta::FromMilliseconds(200));

  // Next scheduled task to run at |start_time_| + 6s
  platform_->RunForPeriod(TimeDelta::FromSeconds(2));
  // Next scheduled task to run at |start_time_| + 8s
  platform_->RunForPeriod(TimeDelta::FromMilliseconds(2100));
  // Next scheduled task to run at |start_time_| + 10s
  platform_->RunForPeriod(TimeDelta::FromMilliseconds(2900));
  // Next scheduled task to run at |start_time_| + 14s (skips a beat)
  platform_->AdvanceClock(TimeDelta::FromMilliseconds(3100));
  platform_->RunUntilIdle();
  // Next scheduled task to run at |start_time_| + 18s (skips a beat)
  platform_->AdvanceClock(TimeDelta::FromSeconds(4));
  platform_->RunUntilIdle();
  // Next scheduled task to run at |start_time_| + 28s (skips 5 beats)
  platform_->AdvanceClock(TimeDelta::FromSeconds(10));
  platform_->RunUntilIdle();

  EXPECT_THAT(next_fire_times_,
              ElementsAre(start_time_ + TimeDelta::FromSeconds(2),
                          start_time_ + TimeDelta::FromSeconds(4),
                          start_time_ + TimeDelta::FromSeconds(6),
                          start_time_ + TimeDelta::FromSeconds(8),
                          start_time_ + TimeDelta::FromSeconds(10),
                          start_time_ + TimeDelta::FromSeconds(14),
                          start_time_ + TimeDelta::FromSeconds(18),
                          start_time_ + TimeDelta::FromSeconds(28)));
}

template <typename TimerFiredClass>
class TimerForTest : public TaskRunnerTimer<TimerFiredClass> {
 public:
  using TimerFiredFunction =
      typename TaskRunnerTimer<TimerFiredClass>::TimerFiredFunction;

  ~TimerForTest() override = default;

  TimerForTest(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
               TimerFiredClass* timer_fired_class,
               TimerFiredFunction timer_fired_function)
      : TaskRunnerTimer<TimerFiredClass>(std::move(task_runner),
                                         timer_fired_class,
                                         timer_fired_function) {}
};

TEST_F(TimerTest, UserSuppliedTaskRunner) {
  scoped_refptr<MainThreadTaskQueue> task_queue(
      platform_->GetMainThreadScheduler()->NewTimerTaskQueue(
          scheduler::MainThreadTaskQueue::QueueType::kFrameThrottleable,
          nullptr));
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      task_queue->CreateTaskRunner(TaskType::kInternalTest);
  TimerForTest<TimerTest> timer(task_runner, this, &TimerTest::CountingTask);
  timer.StartOneShot(TimeDelta(), FROM_HERE);

  // Make sure the task was posted on taskRunner.
  EXPECT_FALSE(task_queue->IsEmpty());
}

TEST_F(TimerTest, RunOnHeapTimer) {
  scoped_refptr<OnHeapTimerOwner::Record> record =
      OnHeapTimerOwner::Record::Create();
  Persistent<OnHeapTimerOwner> owner =
      new OnHeapTimerOwner(record, GetTaskRunner());

  owner->StartOneShot(TimeDelta(), FROM_HERE);

  EXPECT_FALSE(record->TimerHasFired());
  platform_->RunUntilIdle();
  EXPECT_TRUE(record->TimerHasFired());
}

TEST_F(TimerTest, DestructOnHeapTimer) {
  scoped_refptr<OnHeapTimerOwner::Record> record =
      OnHeapTimerOwner::Record::Create();
  Persistent<OnHeapTimerOwner> owner =
      new OnHeapTimerOwner(record, GetTaskRunner());

  record->Dispose();
  owner->StartOneShot(TimeDelta(), FROM_HERE);

  owner = nullptr;
  ThreadState::Current()->CollectGarbage(
      BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
      BlinkGC::kEagerSweeping, BlinkGC::GCReason::kForcedGC);
  EXPECT_TRUE(record->OwnerIsDestructed());

  EXPECT_FALSE(record->TimerHasFired());
  platform_->RunUntilIdle();
  EXPECT_FALSE(record->TimerHasFired());
}

TEST_F(TimerTest, MarkOnHeapTimerAsUnreachable) {
  scoped_refptr<OnHeapTimerOwner::Record> record =
      OnHeapTimerOwner::Record::Create();
  Persistent<OnHeapTimerOwner> owner =
      new OnHeapTimerOwner(record, GetTaskRunner());

  record->Dispose();
  owner->StartOneShot(TimeDelta(), FROM_HERE);

  owner = nullptr;
  ThreadState::Current()->CollectGarbage(
      BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
      BlinkGC::kLazySweeping, BlinkGC::GCReason::kForcedGC);
  EXPECT_FALSE(record->OwnerIsDestructed());

  {
    GCForbiddenScope scope;
    EXPECT_FALSE(record->TimerHasFired());
    platform_->RunUntilIdle();
    EXPECT_FALSE(record->TimerHasFired());
    EXPECT_FALSE(record->OwnerIsDestructed());
  }
}

namespace {

class TaskObserver : public base::MessageLoop::TaskObserver {
 public:
  TaskObserver(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      std::vector<scoped_refptr<base::SingleThreadTaskRunner>>* run_order)
      : task_runner_(std::move(task_runner)), run_order_(run_order) {}

  void WillProcessTask(const base::PendingTask&) override {}

  void DidProcessTask(const base::PendingTask&) override {
    run_order_->push_back(task_runner_);
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::vector<scoped_refptr<base::SingleThreadTaskRunner>>* run_order_;
};

}  // namespace

TEST_F(TimerTest, MoveToNewTaskRunnerOneShot) {
  std::vector<scoped_refptr<base::SingleThreadTaskRunner>> run_order;

  scoped_refptr<MainThreadTaskQueue> task_queue1(
      platform_->GetMainThreadScheduler()->NewTimerTaskQueue(
          scheduler::MainThreadTaskQueue::QueueType::kFrameThrottleable,
          nullptr));
  scoped_refptr<base::SingleThreadTaskRunner> task_runner1 =
      task_queue1->CreateTaskRunner(TaskType::kInternalTest);
  TaskObserver task_observer1(task_runner1, &run_order);
  task_queue1->AddTaskObserver(&task_observer1);

  scoped_refptr<MainThreadTaskQueue> task_queue2(
      platform_->GetMainThreadScheduler()->NewTimerTaskQueue(
          scheduler::MainThreadTaskQueue::QueueType::kFrameThrottleable,
          nullptr));
  scoped_refptr<base::SingleThreadTaskRunner> task_runner2 =
      task_queue2->CreateTaskRunner(TaskType::kInternalTest);
  TaskObserver task_observer2(task_runner2, &run_order);
  task_queue2->AddTaskObserver(&task_observer2);

  TimerForTest<TimerTest> timer(task_runner1, this, &TimerTest::CountingTask);

  TimeTicks start_time = CurrentTimeTicks();

  timer.StartOneShot(TimeDelta::FromSeconds(1), FROM_HERE);

  platform_->RunForPeriod(TimeDelta::FromMilliseconds(500));

  timer.MoveToNewTaskRunner(task_runner2);

  platform_->RunUntilIdle();

  EXPECT_THAT(run_times_, ElementsAre(start_time + TimeDelta::FromSeconds(1)));

  EXPECT_THAT(run_order, ElementsAre(task_runner2));

  EXPECT_TRUE(task_queue1->IsEmpty());
  EXPECT_TRUE(task_queue2->IsEmpty());
}

TEST_F(TimerTest, MoveToNewTaskRunnerRepeating) {
  std::vector<scoped_refptr<base::SingleThreadTaskRunner>> run_order;

  scoped_refptr<MainThreadTaskQueue> task_queue1(
      platform_->GetMainThreadScheduler()->NewTimerTaskQueue(
          scheduler::MainThreadTaskQueue::QueueType::kFrameThrottleable,
          nullptr));
  scoped_refptr<base::SingleThreadTaskRunner> task_runner1 =
      task_queue1->CreateTaskRunner(TaskType::kInternalTest);
  TaskObserver task_observer1(task_runner1, &run_order);
  task_queue1->AddTaskObserver(&task_observer1);

  scoped_refptr<MainThreadTaskQueue> task_queue2(
      platform_->GetMainThreadScheduler()->NewTimerTaskQueue(
          scheduler::MainThreadTaskQueue::QueueType::kFrameThrottleable,
          nullptr));
  scoped_refptr<base::SingleThreadTaskRunner> task_runner2 =
      task_queue2->CreateTaskRunner(TaskType::kInternalTest);
  TaskObserver task_observer2(task_runner2, &run_order);
  task_queue2->AddTaskObserver(&task_observer2);

  TimerForTest<TimerTest> timer(task_runner1, this, &TimerTest::CountingTask);

  TimeTicks start_time = CurrentTimeTicks();

  timer.StartRepeating(TimeDelta::FromSeconds(1), FROM_HERE);

  platform_->RunForPeriod(TimeDelta::FromMilliseconds(2500));

  timer.MoveToNewTaskRunner(task_runner2);

  platform_->RunForPeriod(TimeDelta::FromSeconds(2));

  EXPECT_THAT(run_times_, ElementsAre(start_time + TimeDelta::FromSeconds(1),
                                      start_time + TimeDelta::FromSeconds(2),
                                      start_time + TimeDelta::FromSeconds(3),
                                      start_time + TimeDelta::FromSeconds(4)));

  EXPECT_THAT(run_order, ElementsAre(task_runner1, task_runner1, task_runner2,
                                     task_runner2));

  EXPECT_TRUE(task_queue1->IsEmpty());
  EXPECT_FALSE(task_queue2->IsEmpty());
}

// This test checks that when inactive timer is moved to a different task
// runner it isn't activated.
TEST_F(TimerTest, MoveToNewTaskRunnerWithoutTasks) {
  scoped_refptr<MainThreadTaskQueue> task_queue1(
      platform_->GetMainThreadScheduler()->NewTimerTaskQueue(
          scheduler::MainThreadTaskQueue::QueueType::kFrameThrottleable,
          nullptr));
  scoped_refptr<base::SingleThreadTaskRunner> task_runner1 =
      task_queue1->CreateTaskRunner(TaskType::kInternalTest);

  scoped_refptr<MainThreadTaskQueue> task_queue2(
      platform_->GetMainThreadScheduler()->NewTimerTaskQueue(
          scheduler::MainThreadTaskQueue::QueueType::kFrameThrottleable,
          nullptr));
  scoped_refptr<base::SingleThreadTaskRunner> task_runner2 =
      task_queue2->CreateTaskRunner(TaskType::kInternalTest);

  TimerForTest<TimerTest> timer(task_runner1, this, &TimerTest::CountingTask);

  platform_->RunUntilIdle();
  EXPECT_TRUE(!run_times_.size());
  EXPECT_TRUE(task_queue1->IsEmpty());
  EXPECT_TRUE(task_queue2->IsEmpty());
}

}  // namespace
}  // namespace blink
