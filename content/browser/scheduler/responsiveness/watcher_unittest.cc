// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/watcher.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/pending_task.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/scheduler/responsiveness/calculator.h"
#include "content/browser/scheduler/responsiveness/native_event_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace responsiveness {

namespace {

struct TaskTiming {
  base::TimeTicks queue_time;
  base::TimeTicks execution_start_time;
  base::TimeTicks execution_finish_time;
};

class FakeCalculator : public Calculator {
 public:
  using Calculator::Calculator;

  void TaskOrEventFinishedOnUIThread(
      base::TimeTicks queue_time,
      base::TimeTicks execution_start_time,
      base::TimeTicks execution_finish_time) override {
    execution_times_ui_.push_back(
        {queue_time, execution_start_time, execution_finish_time});
  }

  void TaskOrEventFinishedOnIOThread(
      base::TimeTicks queue_time,
      base::TimeTicks execution_start_time,
      base::TimeTicks execution_finish_time) override {
    base::AutoLock l(io_thread_lock_);
    execution_times_io_.push_back(
        {queue_time, execution_start_time, execution_finish_time});
  }

  int NumTasksOnUIThread() {
    return static_cast<int>(execution_times_ui_.size());
  }
  std::vector<TaskTiming>& ExecutionTimesUIThread() {
    return execution_times_ui_;
  }
  int NumTasksOnIOThread() {
    base::AutoLock l(io_thread_lock_);
    return static_cast<int>(execution_times_io_.size());
  }
  std::vector<TaskTiming>& ExecutionTimesIOThread() {
    base::AutoLock l(io_thread_lock_);
    return execution_times_io_;
  }

 private:
  std::vector<TaskTiming> execution_times_ui_;
  base::Lock io_thread_lock_;
  std::vector<TaskTiming> execution_times_io_;
};

class FakeMetricSource : public MetricSource {
 public:
  FakeMetricSource(Delegate* delegate, bool register_message_loop_observer)
      : MetricSource(delegate),
        register_message_loop_observer_(register_message_loop_observer) {}
  ~FakeMetricSource() override {}

  void RegisterMessageLoopObserverUI() override {
    if (register_message_loop_observer_)
      MetricSource::RegisterMessageLoopObserverUI();
  }
  void RegisterMessageLoopObserverIO() override {
    if (register_message_loop_observer_)
      MetricSource::RegisterMessageLoopObserverIO();
  }

  std::unique_ptr<NativeEventObserver> CreateNativeEventObserver() override {
    return nullptr;
  }

 private:
  bool register_message_loop_observer_;
};

class FakeWatcher : public Watcher {
 public:
  std::unique_ptr<Calculator> CreateCalculator() override {
    std::unique_ptr<FakeCalculator> calculator =
        std::make_unique<FakeCalculator>(nullptr);
    calculator_ = calculator.get();
    return calculator;
  }

  std::unique_ptr<MetricSource> CreateMetricSource() override {
    return std::make_unique<FakeMetricSource>(this,
                                              register_message_loop_observer_);
  }

  FakeWatcher(bool register_message_loop_observer)
      : Watcher(),
        register_message_loop_observer_(register_message_loop_observer) {}

  int NumTasksOnUIThread() { return calculator_->NumTasksOnUIThread(); }
  std::vector<TaskTiming>& ExecutionTimesUIThread() {
    return calculator_->ExecutionTimesUIThread();
  }
  std::vector<TaskTiming>& ExecutionTimesIOThread() {
    return calculator_->ExecutionTimesIOThread();
  }
  int NumTasksOnIOThread() { return calculator_->NumTasksOnIOThread(); }

 private:
  ~FakeWatcher() override {}
  raw_ptr<FakeCalculator> calculator_ = nullptr;
  bool register_message_loop_observer_ = false;
};

}  // namespace

class ResponsivenessWatcherTest : public testing::Test {
 public:
  void SetUp() override {
    // Watcher's constructor posts a task to IO thread, which in the unit test
    // is also this thread. Regardless, we need to let those tasks finish.
    watcher_ = scoped_refptr<FakeWatcher>(
        new FakeWatcher(/*register_message_loop_observer=*/false));
    watcher_->SetUp();
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    watcher_->Destroy();
    watcher_.reset();
  }

 protected:
  // This member sets up BrowserThread::IO and BrowserThread::UI. It must be the
  // first member, as other members may depend on these abstractions.
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<FakeWatcher> watcher_;
};

// Test that tasks are forwarded to calculator.
TEST_F(ResponsivenessWatcherTest, TaskForwarding) {
  for (int i = 0; i < 3; ++i) {
    base::PendingTask task(FROM_HERE, base::OnceClosure(),
                           /*queue_time=*/base::TimeTicks::Now(),
                           /*delayed_run_time=*/base::TimeTicks());
    watcher_->WillRunTaskOnUIThread(&task,
                                    /* was_blocked_or_low_priority= */ false);
    watcher_->DidRunTaskOnUIThread(&task);
  }
  EXPECT_EQ(3, watcher_->NumTasksOnUIThread());
  EXPECT_EQ(0, watcher_->NumTasksOnIOThread());

  for (int i = 0; i < 4; ++i) {
    base::PendingTask task(FROM_HERE, base::OnceClosure(),
                           /*queue_time=*/base::TimeTicks::Now(),
                           /*delayed_run_time=*/base::TimeTicks());
    watcher_->WillRunTaskOnIOThread(&task,
                                    /* was_blocked_or_low_priority= */ false);
    watcher_->DidRunTaskOnIOThread(&task);
  }
  EXPECT_EQ(3, watcher_->NumTasksOnUIThread());
  EXPECT_EQ(4, watcher_->NumTasksOnIOThread());
}

// Test that nested tasks are not forwarded to the calculator.
TEST_F(ResponsivenessWatcherTest, TaskNesting) {
  base::PendingTask task1(FROM_HERE, base::OnceClosure(),
                          /*queue_time=*/base::TimeTicks::Now(),
                          /*delayed_run_time=*/base::TimeTicks());
  base::PendingTask task2(FROM_HERE, base::OnceClosure(),
                          /*queue_time=*/base::TimeTicks::Now(),
                          /*delayed_run_time=*/base::TimeTicks());
  task_environment_.FastForwardBy(base::Milliseconds(1));
  base::PendingTask task3(FROM_HERE, base::OnceClosure(),
                          /*queue_time=*/base::TimeTicks::Now(),
                          /*delayed_run_time=*/base::TimeTicks());

  const base::TimeTicks task_1_execution_start_time = base::TimeTicks::Now();
  watcher_->WillRunTaskOnUIThread(&task1,
                                  /* was_blocked_or_low_priority= */ false);
  watcher_->WillRunTaskOnUIThread(&task2,
                                  /* was_blocked_or_low_priority= */ false);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  const base::TimeTicks task_3_execution_start_time = base::TimeTicks::Now();
  EXPECT_EQ(task_1_execution_start_time + base::Milliseconds(1),
            task_3_execution_start_time);
  watcher_->WillRunTaskOnUIThread(&task3,
                                  /* was_blocked_or_low_priority= */ false);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  const base::TimeTicks task_3_execution_finish_time = base::TimeTicks::Now();
  watcher_->DidRunTaskOnUIThread(&task3);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  watcher_->DidRunTaskOnUIThread(&task2);
  watcher_->DidRunTaskOnUIThread(&task1);

  ASSERT_EQ(1, watcher_->NumTasksOnUIThread());

  // The innermost task should be the one that is passed through, as it didn't
  // cause reentrancy.
  const TaskTiming task_timing = watcher_->ExecutionTimesUIThread()[0];
  EXPECT_EQ(task3.queue_time, task_timing.queue_time);
  EXPECT_EQ(task_3_execution_start_time, task_timing.execution_start_time);
  EXPECT_EQ(task_3_execution_finish_time, task_timing.execution_finish_time);

  EXPECT_EQ(0, watcher_->NumTasksOnIOThread());
}

// Test that native events use execution time instead of queue + execution time.
TEST_F(ResponsivenessWatcherTest, NativeEvents) {
  const base::TimeTicks start_time = base::TimeTicks::Now();

  void* opaque_identifier = reinterpret_cast<void*>(0x1234);
  watcher_->WillRunEventOnUIThread(opaque_identifier);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  const base::TimeTicks finish_time = base::TimeTicks::Now();
  watcher_->DidRunEventOnUIThread(opaque_identifier);

  ASSERT_EQ(1, watcher_->NumTasksOnUIThread());
  const TaskTiming task_timing = watcher_->ExecutionTimesUIThread()[0];
  EXPECT_EQ(task_timing.queue_time, start_time);
  EXPECT_EQ(task_timing.execution_start_time, start_time);
  EXPECT_EQ(task_timing.execution_finish_time, finish_time);

  EXPECT_EQ(0, watcher_->NumTasksOnIOThread());
}

// Test that the queue duration of a blocked or low priority task is zero.
TEST_F(ResponsivenessWatcherTest, BlockedOrLowPriorityTask) {
  base::PendingTask task(FROM_HERE, base::OnceClosure(),
                         /*queue_time=*/base::TimeTicks::Now(),
                         /*delayed_run_time=*/base::TimeTicks());
  task_environment_.FastForwardBy(base::Seconds(1));

  const base::TimeTicks execution_start_time = base::TimeTicks::Now();
  watcher_->WillRunTaskOnUIThread(&task,
                                  /* was_blocked_or_low_priority= */ true);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  const base::TimeTicks execution_finish_time = base::TimeTicks::Now();
  watcher_->DidRunTaskOnUIThread(&task);

  ASSERT_EQ(1, watcher_->NumTasksOnUIThread());
  const TaskTiming task_timing = watcher_->ExecutionTimesUIThread()[0];
  // The queue time should be equal to the execution start time, to simulate
  // that the task did not spend any time in a queue. This is the desired
  // behavior for a task that was blocked or low priority.
  EXPECT_EQ(task_timing.queue_time, execution_start_time);
  EXPECT_EQ(task_timing.execution_start_time, execution_start_time);
  EXPECT_EQ(task_timing.execution_finish_time, execution_finish_time);

  EXPECT_EQ(0, watcher_->NumTasksOnIOThread());
}

// Test that the queue duration of a delayed task is zero.
TEST_F(ResponsivenessWatcherTest, DelayedTask) {
  base::PendingTask task(FROM_HERE, base::OnceClosure(),
                         /*queue_time=*/base::TimeTicks::Now(),
                         /*delayed_run_time=*/base::TimeTicks::Now());
  task_environment_.FastForwardBy(base::Seconds(1));

  const base::TimeTicks execution_start_time = base::TimeTicks::Now();
  watcher_->WillRunTaskOnUIThread(&task,
                                  /* was_blocked_or_low_priority= */ false);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  const base::TimeTicks execution_finish_time = base::TimeTicks::Now();
  watcher_->DidRunTaskOnUIThread(&task);

  ASSERT_EQ(1, watcher_->NumTasksOnUIThread());
  const TaskTiming task_timing = watcher_->ExecutionTimesUIThread()[0];
  // The queue time should be equal to the execution start time, to simulate
  // that the task did not spend any time in a queue. This is the desired
  // behavior for a delayed task.
  EXPECT_EQ(task_timing.queue_time, execution_start_time);
  EXPECT_EQ(task_timing.execution_start_time, execution_start_time);
  EXPECT_EQ(task_timing.execution_finish_time, execution_finish_time);

  EXPECT_EQ(0, watcher_->NumTasksOnIOThread());
}

class ResponsivenessWatcherRealIOThreadTest : public testing::Test {
 public:
  ResponsivenessWatcherRealIOThreadTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

  void SetUp() override {
    // Watcher's constructor posts a task to IO thread. We need to let those
    // tasks finish.
    watcher_ = scoped_refptr<FakeWatcher>(
        new FakeWatcher(/*register_message_loop_observer=*/true));
    watcher_->SetUp();
    task_environment_.RunIOThreadUntilIdle();
  }

  void TearDown() override {
    watcher_->Destroy();
    watcher_.reset();

    // Destroy a task onto the IO thread, which posts back to the UI thread
    // to complete destruction.
    task_environment_.RunIOThreadUntilIdle();
    task_environment_.RunUntilIdle();
  }

 protected:
  // This member sets up BrowserThread::IO and BrowserThread::UI. It must be the
  // first member, as other members may depend on these abstractions.
  content::BrowserTaskEnvironment task_environment_;

  scoped_refptr<FakeWatcher> watcher_;
};

TEST_F(ResponsivenessWatcherRealIOThreadTest, MessageLoopObserver) {
  // Post a do-nothing task onto the UI thread.
  GetUIThreadTaskRunner({})->PostTask(FROM_HERE, base::DoNothing());

  // Post a do-nothing task onto the IO thread.
  content::GetIOThreadTaskRunner({})->PostTask(FROM_HERE, base::DoNothing());

  // Post a task onto the IO thread that hops back to the UI thread. This
  // guarantees that both of the do-nothing tasks have already been processed.
  base::RunLoop run_loop;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::OnceClosure quit_closure) {
                       GetUIThreadTaskRunner({})->PostTask(
                           FROM_HERE, std::move(quit_closure));
                     },
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_GE(watcher_->NumTasksOnUIThread(), 1);
  const TaskTiming ui_thread_task_timing =
      watcher_->ExecutionTimesUIThread()[0];
  EXPECT_FALSE(ui_thread_task_timing.queue_time.is_null());
  EXPECT_FALSE(ui_thread_task_timing.execution_start_time.is_null());
  EXPECT_FALSE(ui_thread_task_timing.execution_finish_time.is_null());

  ASSERT_GE(watcher_->NumTasksOnIOThread(), 1);
  const TaskTiming io_thread_task_timing =
      watcher_->ExecutionTimesUIThread()[0];
  EXPECT_FALSE(io_thread_task_timing.queue_time.is_null());
  EXPECT_FALSE(io_thread_task_timing.execution_start_time.is_null());
  EXPECT_FALSE(io_thread_task_timing.execution_finish_time.is_null());
}

}  // namespace responsiveness
}  // namespace content
