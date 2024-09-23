// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/timer.h"

#include <memory>
#include <queue>

#include "base/memory/raw_ptr.h"
#include "base/task/common/lazy_now.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/thread_state_scopes.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

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
    platform_->AdvanceClock(base::Seconds(10));
    start_time_ = Now();
  }

  base::TimeTicks Now() { return platform_->test_task_runner()->NowTicks(); }

  void CountingTask(TimerBase*) { run_times_.push_back(Now()); }

  void RecordNextFireTimeTask(TimerBase* timer) {
    next_fire_times_.push_back(Now() + timer->NextFireInterval());
  }

  void RunUntilDeadline(base::TimeTicks deadline) {
    base::TimeDelta period = deadline - Now();
    EXPECT_GE(period, base::TimeDelta());
    platform_->RunForPeriod(period);
  }

  // Returns false if there are no pending delayed tasks, otherwise sets |time|
  // to the delay in seconds till the next pending delayed task is scheduled to
  // fire.
  bool TimeTillNextDelayedTask(base::TimeDelta* time) const {
    base::LazyNow lazy_now(platform_->NowTicks());
    auto* scheduler_helper =
        platform_->GetMainThreadScheduler()->GetSchedulerHelperForTesting();
    scheduler_helper->ReclaimMemory();
    auto wake_up = scheduler_helper->GetNextWakeUp();
    if (!wake_up)
      return false;
    *time = wake_up->time - lazy_now.Now();
    return true;
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
    return task_runner_;
  }

 protected:
  base::TimeTicks start_time_;
  WTF::Vector<base::TimeTicks> run_times_;
  WTF::Vector<base::TimeTicks> next_fire_times_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::test::TaskEnvironment task_environment_;
};

class OnHeapTimerOwner final : public GarbageCollected<OnHeapTimerOwner> {
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

  void StartOneShot(base::TimeDelta interval, const base::Location& caller) {
    timer_.StartOneShot(interval, caller);
  }

  void Trace(Visitor* visitor) const { visitor->Trace(timer_); }

 private:
  void Fired(TimerBase*) {
    EXPECT_FALSE(record_->IsDisposed());
    record_->SetTimerHasFired();
  }

  HeapTaskRunnerTimer<OnHeapTimerOwner> timer_;
  scoped_refptr<Record> record_;
};

TEST_F(TimerTest, StartOneShot_Zero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::TimeDelta(), FROM_HERE);

  base::TimeDelta run_time;
  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_));
}

TEST_F(TimerTest, StartOneShot_ZeroAndCancel) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::TimeDelta(), FROM_HERE);

  base::TimeDelta run_time;
  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  timer.Stop();

  platform_->RunUntilIdle();
  EXPECT_FALSE(run_times_.size());
}

TEST_F(TimerTest, StartOneShot_ZeroAndCancelThenRepost) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::TimeDelta(), FROM_HERE);

  base::TimeDelta run_time;
  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  timer.Stop();

  platform_->RunUntilIdle();
  EXPECT_FALSE(run_times_.size());

  timer.StartOneShot(base::TimeDelta(), FROM_HERE);

  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_));
}

TEST_F(TimerTest, StartOneShot_Zero_RepostingAfterRunning) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::TimeDelta(), FROM_HERE);

  base::TimeDelta run_time;
  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_));

  timer.StartOneShot(base::TimeDelta(), FROM_HERE);

  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_, start_time_));
}

TEST_F(TimerTest, StartOneShot_NonZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::Seconds(10), FROM_HERE);

  base::TimeDelta run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(base::Seconds(10), run_time);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + base::Seconds(10)));
}

TEST_F(TimerTest, StartOneShot_NonZeroAndCancel) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::Seconds(10), FROM_HERE);

  base::TimeDelta run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(base::Seconds(10), run_time);

  timer.Stop();
  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_FALSE(run_times_.size());
}

TEST_F(TimerTest, StartOneShot_NonZeroAndCancelThenRepost) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::Seconds(10), FROM_HERE);

  base::TimeDelta run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(base::Seconds(10), run_time);

  timer.Stop();
  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_FALSE(run_times_.size());

  base::TimeTicks second_post_time = Now();
  timer.StartOneShot(base::Seconds(10), FROM_HERE);

  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(base::Seconds(10), run_time);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(second_post_time + base::Seconds(10)));
}

TEST_F(TimerTest, StartOneShot_NonZero_RepostingAfterRunning) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::Seconds(10), FROM_HERE);

  base::TimeDelta run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(base::Seconds(10), run_time);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + base::Seconds(10)));

  timer.StartOneShot(base::Seconds(20), FROM_HERE);

  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(base::Seconds(20), run_time);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + base::Seconds(10),
                                      start_time_ + base::Seconds(30)));
}

TEST_F(TimerTest, PostingTimerTwiceWithSameRunTimeDoesNothing) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::Seconds(10), FROM_HERE);
  timer.StartOneShot(base::Seconds(10), FROM_HERE);

  base::TimeDelta run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(base::Seconds(10), run_time);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + base::Seconds(10)));
}

TEST_F(TimerTest, PostingTimerTwiceWithNewerRunTimeCancelsOriginalTask) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::Seconds(10), FROM_HERE);
  timer.StartOneShot(base::TimeDelta(), FROM_HERE);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + base::Seconds(0)));
}

TEST_F(TimerTest, PostingTimerTwiceWithLaterRunTimeCancelsOriginalTask) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::TimeDelta(), FROM_HERE);
  timer.StartOneShot(base::Seconds(10), FROM_HERE);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + base::Seconds(10)));
}

TEST_F(TimerTest, StartRepeatingTask) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(base::Seconds(1), FROM_HERE);

  base::TimeDelta run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(base::Seconds(1), run_time);

  RunUntilDeadline(start_time_ + base::Milliseconds(5500));
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + base::Seconds(1),
                                      start_time_ + base::Seconds(2),
                                      start_time_ + base::Seconds(3),
                                      start_time_ + base::Seconds(4),
                                      start_time_ + base::Seconds(5)));
}

TEST_F(TimerTest, StartRepeatingTask_ThenCancel) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(base::Seconds(1), FROM_HERE);

  base::TimeDelta run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(base::Seconds(1), run_time);

  RunUntilDeadline(start_time_ + base::Milliseconds(2500));
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + base::Seconds(1),
                                      start_time_ + base::Seconds(2)));

  timer.Stop();
  platform_->RunUntilIdle();

  EXPECT_THAT(run_times_, ElementsAre(start_time_ + base::Seconds(1),
                                      start_time_ + base::Seconds(2)));
}

TEST_F(TimerTest, StartRepeatingTask_ThenPostOneShot) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(base::Seconds(1), FROM_HERE);

  base::TimeDelta run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_EQ(base::Seconds(1), run_time);

  RunUntilDeadline(start_time_ + base::Milliseconds(2500));
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + base::Seconds(1),
                                      start_time_ + base::Seconds(2)));

  timer.StartOneShot(base::TimeDelta(), FROM_HERE);
  platform_->RunUntilIdle();

  EXPECT_THAT(run_times_, ElementsAre(start_time_ + base::Seconds(1),
                                      start_time_ + base::Seconds(2),
                                      start_time_ + base::Milliseconds(2500)));
}

TEST_F(TimerTest, IsActive_NeverPosted) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);

  EXPECT_FALSE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_OneShotZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::TimeDelta(), FROM_HERE);

  EXPECT_TRUE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_OneShotNonZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::Seconds(10), FROM_HERE);

  EXPECT_TRUE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_Repeating) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(base::Seconds(1), FROM_HERE);

  EXPECT_TRUE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_OneShotZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::TimeDelta(), FROM_HERE);

  platform_->RunUntilIdle();
  EXPECT_FALSE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_OneShotNonZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::Seconds(10), FROM_HERE);

  platform_->RunUntilIdle();
  EXPECT_FALSE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_Repeating) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(base::Seconds(1), FROM_HERE);

  RunUntilDeadline(start_time_ + base::Seconds(10));
  EXPECT_TRUE(timer.IsActive());  // It should run until cancelled.
}

TEST_F(TimerTest, NextFireInterval_OneShotZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::TimeDelta(), FROM_HERE);

  EXPECT_TRUE(timer.NextFireInterval().is_zero());
}

TEST_F(TimerTest, NextFireInterval_OneShotNonZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::Seconds(10), FROM_HERE);

  EXPECT_EQ(base::Seconds(10), timer.NextFireInterval());
}

TEST_F(TimerTest, NextFireInterval_OneShotNonZero_AfterAFewSeconds) {
  platform_->SetAutoAdvanceNowToPendingTasks(false);

  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::Seconds(10), FROM_HERE);

  platform_->AdvanceClock(base::Seconds(2));
  EXPECT_EQ(base::Seconds(8), timer.NextFireInterval());
}

TEST_F(TimerTest, NextFireInterval_Repeating) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(base::Seconds(20), FROM_HERE);

  EXPECT_EQ(base::Seconds(20), timer.NextFireInterval());
}

TEST_F(TimerTest, RepeatInterval_NeverStarted) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);

  EXPECT_TRUE(timer.RepeatInterval().is_zero());
}

TEST_F(TimerTest, RepeatInterval_OneShotZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::TimeDelta(), FROM_HERE);

  EXPECT_TRUE(timer.RepeatInterval().is_zero());
}

TEST_F(TimerTest, RepeatInterval_OneShotNonZero) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartOneShot(base::Seconds(10), FROM_HERE);

  EXPECT_TRUE(timer.RepeatInterval().is_zero());
}

TEST_F(TimerTest, RepeatInterval_Repeating) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(base::Seconds(20), FROM_HERE);

  EXPECT_EQ(base::Seconds(20), timer.RepeatInterval());
}

TEST_F(TimerTest, AugmentRepeatInterval) {
  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(base::Seconds(10), FROM_HERE);
  EXPECT_EQ(base::Seconds(10), timer.RepeatInterval());
  EXPECT_EQ(base::Seconds(10), timer.NextFireInterval());

  platform_->AdvanceClock(base::Seconds(2));
  timer.AugmentRepeatInterval(base::Seconds(10));

  EXPECT_EQ(base::Seconds(20), timer.RepeatInterval());
  EXPECT_EQ(base::Seconds(18), timer.NextFireInterval());

  RunUntilDeadline(start_time_ + base::Seconds(50));
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + base::Seconds(20),
                                      start_time_ + base::Seconds(40)));
}

TEST_F(TimerTest, AugmentRepeatInterval_TimerFireDelayed) {
  platform_->SetAutoAdvanceNowToPendingTasks(false);

  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::CountingTask);
  timer.StartRepeating(base::Seconds(10), FROM_HERE);
  EXPECT_EQ(base::Seconds(10), timer.RepeatInterval());
  EXPECT_EQ(base::Seconds(10), timer.NextFireInterval());

  platform_->AdvanceClock(base::Seconds(123));  // Make the timer long overdue.
  timer.AugmentRepeatInterval(base::Seconds(10));

  EXPECT_EQ(base::Seconds(20), timer.RepeatInterval());
  // The timer is overdue so it should be scheduled to fire immediatly.
  EXPECT_TRUE(timer.NextFireInterval().is_zero());
}

TEST_F(TimerTest, RepeatingTimerDoesNotDrift) {
  platform_->SetAutoAdvanceNowToPendingTasks(false);

  TaskRunnerTimer<TimerTest> timer(GetTaskRunner(), this,
                                   &TimerTest::RecordNextFireTimeTask);
  timer.StartRepeating(base::Seconds(2), FROM_HERE);

  RecordNextFireTimeTask(
      &timer);  // Next scheduled task to run at |start_time_| + 2s

  // Simulate timer firing early. Next scheduled task to run at
  // |start_time_| + 4s
  platform_->AdvanceClock(base::Milliseconds(1900));
  RunUntilDeadline(Now() + base::Milliseconds(200));

  // Next scheduled task to run at |start_time_| + 6s
  platform_->RunForPeriod(base::Seconds(2));
  // Next scheduled task to run at |start_time_| + 8s
  platform_->RunForPeriod(base::Milliseconds(2100));
  // Next scheduled task to run at |start_time_| + 10s
  platform_->RunForPeriod(base::Milliseconds(2900));
  // Next scheduled task to run at |start_time_| + 12s
  platform_->AdvanceClock(base::Milliseconds(1800));
  platform_->RunUntilIdle();
  // Next scheduled task to run at |start_time_| + 14s
  platform_->AdvanceClock(base::Milliseconds(1900));
  platform_->RunUntilIdle();
  // Next scheduled task to run at |start_time_| + 18s (skips a beat)
  platform_->AdvanceClock(base::Milliseconds(50));
  platform_->RunUntilIdle();
  // Next scheduled task to run at |start_time_| + 28s (skips 5 beats)
  platform_->AdvanceClock(base::Seconds(10));
  platform_->RunUntilIdle();

  EXPECT_THAT(
      next_fire_times_,
      ElementsAre(
          start_time_ + base::Seconds(2), start_time_ + base::Seconds(4),
          start_time_ + base::Seconds(6), start_time_ + base::Seconds(8),
          start_time_ + base::Seconds(10), start_time_ + base::Seconds(12),
          start_time_ + base::Seconds(14), start_time_ + base::Seconds(24)));
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
      platform_->GetMainThreadScheduler()->NewThrottleableTaskQueueForTest(
          nullptr));
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      task_queue->CreateTaskRunner(TaskType::kInternalTest);
  TimerForTest<TimerTest> timer(task_runner, this, &TimerTest::CountingTask);
  timer.StartOneShot(base::TimeDelta(), FROM_HERE);

  // Make sure the task was posted on taskRunner.
  EXPECT_FALSE(task_queue->IsEmpty());
}

TEST_F(TimerTest, RunOnHeapTimer) {
  scoped_refptr<OnHeapTimerOwner::Record> record =
      OnHeapTimerOwner::Record::Create();
  Persistent<OnHeapTimerOwner> owner =
      MakeGarbageCollected<OnHeapTimerOwner>(record, GetTaskRunner());

  owner->StartOneShot(base::TimeDelta(), FROM_HERE);

  EXPECT_FALSE(record->TimerHasFired());
  platform_->RunUntilIdle();
  EXPECT_TRUE(record->TimerHasFired());
}

TEST_F(TimerTest, DestructOnHeapTimer) {
  scoped_refptr<OnHeapTimerOwner::Record> record =
      OnHeapTimerOwner::Record::Create();
  Persistent<OnHeapTimerOwner> owner =
      MakeGarbageCollected<OnHeapTimerOwner>(record, GetTaskRunner());

  record->Dispose();
  owner->StartOneShot(base::TimeDelta(), FROM_HERE);

  owner = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  EXPECT_TRUE(record->OwnerIsDestructed());

  EXPECT_FALSE(record->TimerHasFired());
  platform_->RunUntilIdle();
  EXPECT_FALSE(record->TimerHasFired());
}

// TODO(1056170): Re-enable test.
TEST_F(TimerTest, DISABLED_MarkOnHeapTimerAsUnreachable) {
  scoped_refptr<OnHeapTimerOwner::Record> record =
      OnHeapTimerOwner::Record::Create();
  Persistent<OnHeapTimerOwner> owner =
      MakeGarbageCollected<OnHeapTimerOwner>(record, GetTaskRunner());

  record->Dispose();
  owner->StartOneShot(base::TimeDelta(), FROM_HERE);

  owner = nullptr;
  // Explicit regular GC call to allow lazy sweeping.
  // TODO(1056170): Needs a specific forced GC call to be able to test the
  // scenario below.
  // ThreadState::Current()->CollectGarbageForTesting(
  //     BlinkGC::CollectionType::kMajor, BlinkGC::kNoHeapPointersOnStack,
  //     BlinkGC::kAtomicMarking, BlinkGC::kConcurrentAndLazySweeping,
  //     BlinkGC::GCReason::kForcedGCForTesting);
  // Since the heap is laziy swept, owner is not yet destructed.
  EXPECT_FALSE(record->OwnerIsDestructed());

  {
    ThreadState::GCForbiddenScope gc_forbidden(ThreadState::Current());
    EXPECT_FALSE(record->TimerHasFired());
    platform_->RunUntilIdle();
    EXPECT_FALSE(record->TimerHasFired());
    EXPECT_FALSE(record->OwnerIsDestructed());
    // ThreadState::Current()->CompleteSweep();
  }
}

namespace {

class TaskObserver : public base::TaskObserver {
 public:
  TaskObserver(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
               Vector<scoped_refptr<base::SingleThreadTaskRunner>>* run_order)
      : task_runner_(std::move(task_runner)), run_order_(run_order) {}

  void WillProcessTask(const base::PendingTask&, bool) override {}

  void DidProcessTask(const base::PendingTask&) override {
    run_order_->push_back(task_runner_);
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  raw_ptr<Vector<scoped_refptr<base::SingleThreadTaskRunner>>> run_order_;
};

}  // namespace

TEST_F(TimerTest, MoveToNewTaskRunnerOneShot) {
  Vector<scoped_refptr<base::SingleThreadTaskRunner>> run_order;

  scoped_refptr<MainThreadTaskQueue> task_queue1(
      platform_->GetMainThreadScheduler()->NewThrottleableTaskQueueForTest(
          nullptr));
  scoped_refptr<base::SingleThreadTaskRunner> task_runner1 =
      task_queue1->CreateTaskRunner(TaskType::kInternalTest);
  TaskObserver task_observer1(task_runner1, &run_order);
  task_queue1->AddTaskObserver(&task_observer1);

  scoped_refptr<MainThreadTaskQueue> task_queue2(
      platform_->GetMainThreadScheduler()->NewThrottleableTaskQueueForTest(
          nullptr));
  scoped_refptr<base::SingleThreadTaskRunner> task_runner2 =
      task_queue2->CreateTaskRunner(TaskType::kInternalTest);
  TaskObserver task_observer2(task_runner2, &run_order);
  task_queue2->AddTaskObserver(&task_observer2);

  TimerForTest<TimerTest> timer(task_runner1, this, &TimerTest::CountingTask);

  base::TimeTicks start_time = Now();

  timer.StartOneShot(base::Seconds(1), FROM_HERE);

  platform_->RunForPeriod(base::Milliseconds(500));

  timer.MoveToNewTaskRunner(task_runner2);

  platform_->RunUntilIdle();

  EXPECT_THAT(run_times_, ElementsAre(start_time + base::Seconds(1)));

  EXPECT_THAT(run_order, ElementsAre(task_runner2));

  EXPECT_TRUE(task_queue1->IsEmpty());
  EXPECT_TRUE(task_queue2->IsEmpty());
}

TEST_F(TimerTest, MoveToNewTaskRunnerRepeating) {
  Vector<scoped_refptr<base::SingleThreadTaskRunner>> run_order;

  scoped_refptr<MainThreadTaskQueue> task_queue1(
      platform_->GetMainThreadScheduler()->NewThrottleableTaskQueueForTest(
          nullptr));
  scoped_refptr<base::SingleThreadTaskRunner> task_runner1 =
      task_queue1->CreateTaskRunner(TaskType::kInternalTest);
  TaskObserver task_observer1(task_runner1, &run_order);
  task_queue1->AddTaskObserver(&task_observer1);

  scoped_refptr<MainThreadTaskQueue> task_queue2(
      platform_->GetMainThreadScheduler()->NewThrottleableTaskQueueForTest(
          nullptr));
  scoped_refptr<base::SingleThreadTaskRunner> task_runner2 =
      task_queue2->CreateTaskRunner(TaskType::kInternalTest);
  TaskObserver task_observer2(task_runner2, &run_order);
  task_queue2->AddTaskObserver(&task_observer2);

  TimerForTest<TimerTest> timer(task_runner1, this, &TimerTest::CountingTask);

  base::TimeTicks start_time = Now();

  timer.StartRepeating(base::Seconds(1), FROM_HERE);

  platform_->RunForPeriod(base::Milliseconds(2500));

  timer.MoveToNewTaskRunner(task_runner2);

  platform_->RunForPeriod(base::Seconds(2));

  EXPECT_THAT(run_times_, ElementsAre(start_time + base::Seconds(1),
                                      start_time + base::Seconds(2),
                                      start_time + base::Seconds(3),
                                      start_time + base::Seconds(4)));

  EXPECT_THAT(run_order, ElementsAre(task_runner1, task_runner1, task_runner2,
                                     task_runner2));

  EXPECT_TRUE(task_queue1->IsEmpty());
  EXPECT_FALSE(task_queue2->IsEmpty());
}

// This test checks that when inactive timer is moved to a different task
// runner it isn't activated.
TEST_F(TimerTest, MoveToNewTaskRunnerWithoutTasks) {
  scoped_refptr<MainThreadTaskQueue> task_queue1(
      platform_->GetMainThreadScheduler()->NewThrottleableTaskQueueForTest(
          nullptr));
  scoped_refptr<base::SingleThreadTaskRunner> task_runner1 =
      task_queue1->CreateTaskRunner(TaskType::kInternalTest);

  scoped_refptr<MainThreadTaskQueue> task_queue2(
      platform_->GetMainThreadScheduler()->NewThrottleableTaskQueueForTest(
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
