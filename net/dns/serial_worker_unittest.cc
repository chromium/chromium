// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/serial_worker.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {
constexpr base::TimeDelta kBackoffInitialDelay = base::Milliseconds(100);
constexpr int kBackoffMultiplyFactor = 2;
constexpr int kMaxRetries = 3;

static const BackoffEntry::Policy kTestBackoffPolicy = {
    0,  // Number of initial errors to ignore without backoff.
    static_cast<int>(
        kBackoffInitialDelay
            .InMilliseconds()),  // Initial delay for backoff in ms.
    kBackoffMultiplyFactor,      // Factor to multiply for exponential backoff.
    0,                           // Fuzzing percentage.
    static_cast<int>(
        base::Seconds(1).InMilliseconds()),  // Maximum time to delay requests
                                             // in ms: 1 second.
    -1,                                      // Don't discard entry.
    false  // Don't use initial delay unless the last was an error.
};

class SerialWorkerTest : public TestWithTaskEnvironment {
 public:
  // The class under test
  class TestSerialWorker : public SerialWorker {
   public:
    class TestWorkItem : public SerialWorker::WorkItem {
     public:
      explicit TestWorkItem(SerialWorkerTest* test) : test_(test) {}

      void DoWork() override {
        ASSERT_TRUE(test_);
        test_->OnWork();
      }

      void FollowupWork(base::OnceClosure closure) override {
        ASSERT_TRUE(test_);
        test_->OnFollowup(std::move(closure));
      }

     private:
      raw_ptr<SerialWorkerTest> test_;
    };

    explicit TestSerialWorker(SerialWorkerTest* t)
        : SerialWorker(/*max_number_of_retries=*/kMaxRetries,
                       &kTestBackoffPolicy),
          test_(t) {}
    ~TestSerialWorker() override = default;

    std::unique_ptr<SerialWorker::WorkItem> CreateWorkItem() override {
      return std::make_unique<TestWorkItem>(test_);
    }

    bool OnWorkFinished(
        std::unique_ptr<SerialWorker::WorkItem> work_item) override {
      CHECK(test_);
      return test_->OnWorkFinished();
    }

   private:
    raw_ptr<SerialWorkerTest> test_;
  };

  SerialWorkerTest(const SerialWorkerTest&) = delete;
  SerialWorkerTest& operator=(const SerialWorkerTest&) = delete;

  // Mocks

  void OnWork() {
    { // Check that OnWork is executed serially.
      base::AutoLock lock(work_lock_);
      EXPECT_FALSE(work_running_) << "`DoWork()` is not called serially!";
      work_running_ = true;
    }
    num_work_calls_observed_++;
    BreakNow("OnWork");
    {
      base::ScopedAllowBaseSyncPrimitivesForTesting
          scoped_allow_base_sync_primitives;
      work_allowed_.Wait();
    }
    // Calling from ThreadPool, but protected by work_allowed_/work_called_.
    output_value_ = input_value_;

    { // This lock might be destroyed after work_called_ is signalled.
      base::AutoLock lock(work_lock_);
      work_running_ = false;
    }
    work_called_.Signal();
  }

  void OnFollowup(base::OnceClosure closure) {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());

    followup_closure_ = std::move(closure);
    BreakNow("OnFollowup");

    if (followup_immediately_)
      CompleteFollowup();
  }

  bool OnWorkFinished() {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    EXPECT_EQ(output_value_, input_value_);
    ++work_finished_calls_;
    BreakNow("OnWorkFinished");
    return on_work_finished_should_report_success_;
  }

 protected:
  void BreakCallback(const std::string& breakpoint) {
    breakpoint_ = breakpoint;
    run_loop_->Quit();
  }

  void BreakNow(const std::string& b) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&SerialWorkerTest::BreakCallback,
                                          base::Unretained(this), b));
  }

  void RunUntilBreak(const std::string& b) {
    base::RunLoop run_loop;
    ASSERT_FALSE(run_loop_);
    run_loop_ = &run_loop;
    run_loop_->Run();
    run_loop_ = nullptr;
    ASSERT_EQ(breakpoint_, b);
  }

  void CompleteFollowup() {
    ASSERT_TRUE(followup_closure_);
    task_runner_->PostTask(FROM_HERE, std::move(followup_closure_));
  }

  SerialWorkerTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        work_allowed_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
        work_called_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                     base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  // Helpers for tests.

  // Lets OnWork run and waits for it to complete. Can only return if OnWork is
  // executed on a concurrent thread. Before calling, OnWork() must already have
  // been started and blocked (ensured by running `RunUntilBreak("OnWork")`).
  void UnblockWork() {
    ASSERT_TRUE(work_running_);
    work_allowed_.Signal();
    work_called_.Wait();
  }

  // test::Test methods
  void SetUp() override {
    task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  }

  void TearDown() override {
    // Cancel the worker to catch if it makes a late DoWork call.
    if (worker_)
      worker_->Cancel();
    // Check if OnWork is stalled.
    EXPECT_FALSE(work_running_) << "OnWork should be done by TearDown";
    // Release it for cleanliness.
    if (work_running_) {
      UnblockWork();
    }
  }

  // Input value read on WorkerPool.
  int input_value_ = 0;
  // Output value written on WorkerPool.
  int output_value_ = -1;
  // The number of times we saw an OnWork call.
  int num_work_calls_observed_ = 0;
  bool on_work_finished_should_report_success_ = true;

  // read is called on WorkerPool so we need to synchronize with it.
  base::WaitableEvent work_allowed_;
  base::WaitableEvent work_called_;

  // Protected by read_lock_. Used to verify that read calls are serialized.
  bool work_running_ = false;
  base::Lock work_lock_;

  int work_finished_calls_ = 0;

  // Task runner for this thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // WatcherDelegate under test.
  std::unique_ptr<TestSerialWorker> worker_ =
      std::make_unique<TestSerialWorker>(this);

  std::string breakpoint_;
  raw_ptr<base::RunLoop> run_loop_ = nullptr;

  bool followup_immediately_ = true;
  base::OnceClosure followup_closure_;
};

TEST_F(SerialWorkerTest, RunWorkMultipleTimes) {
  for (int i = 0; i < 3; ++i) {
    ++input_value_;
    worker_->WorkNow();
    RunUntilBreak("OnWork");
    EXPECT_EQ(work_finished_calls_, i);
    UnblockWork();
    RunUntilBreak("OnFollowup");
    RunUntilBreak("OnWorkFinished");
    EXPECT_EQ(work_finished_calls_, i + 1);

    EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
  }
}

TEST_F(SerialWorkerTest, TriggerTwoTimesBeforeRun) {
  // Schedule two calls. OnWork checks if it is called serially.
  ++input_value_;
  worker_->WorkNow();
  // Work is blocked, so this will have to induce re-work
  worker_->WorkNow();

  // Expect 2 cycles through work.
  RunUntilBreak("OnWork");
  UnblockWork();
  RunUntilBreak("OnWork");
  UnblockWork();
  RunUntilBreak("OnFollowup");
  RunUntilBreak("OnWorkFinished");

  EXPECT_EQ(work_finished_calls_, 1);

  // No more tasks should remain.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

TEST_F(SerialWorkerTest, TriggerThreeTimesBeforeRun) {
  // Schedule two calls. OnWork checks if it is called serially.
  ++input_value_;
  worker_->WorkNow();
  // Work is blocked, so this will have to induce re-work
  worker_->WorkNow();
  // Repeat work is already scheduled, so this should be a noop.
  worker_->WorkNow();

  // Expect 2 cycles through work.
  RunUntilBreak("OnWork");
  UnblockWork();
  RunUntilBreak("OnWork");
  UnblockWork();
  RunUntilBreak("OnFollowup");
  RunUntilBreak("OnWorkFinished");

  EXPECT_EQ(work_finished_calls_, 1);

  // No more tasks should remain.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

TEST_F(SerialWorkerTest, DelayFollowupCompletion) {
  followup_immediately_ = false;
  worker_->WorkNow();

  RunUntilBreak("OnWork");
  UnblockWork();
  RunUntilBreak("OnFollowup");
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());

  CompleteFollowup();
  RunUntilBreak("OnWorkFinished");

  EXPECT_EQ(work_finished_calls_, 1);

  // No more tasks should remain.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

TEST_F(SerialWorkerTest, RetriggerDuringRun) {
  // Trigger work and wait until blocked.
  worker_->WorkNow();
  RunUntilBreak("OnWork");

  worker_->WorkNow();
  worker_->WorkNow();

  // Expect a second work cycle after completion of current.
  UnblockWork();
  RunUntilBreak("OnWork");
  UnblockWork();
  RunUntilBreak("OnFollowup");
  RunUntilBreak("OnWorkFinished");

  EXPECT_EQ(work_finished_calls_, 1);

  // No more tasks should remain.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

TEST_F(SerialWorkerTest, RetriggerDuringFollowup) {
  // Trigger work and wait until blocked on followup.
  followup_immediately_ = false;
  worker_->WorkNow();
  RunUntilBreak("OnWork");
  UnblockWork();
  RunUntilBreak("OnFollowup");

  worker_->WorkNow();
  worker_->WorkNow();

  // Expect a second work cycle after completion of followup.
  CompleteFollowup();
  RunUntilBreak("OnWork");
  UnblockWork();
  RunUntilBreak("OnFollowup");
  CompleteFollowup();
  RunUntilBreak("OnWorkFinished");

  EXPECT_EQ(work_finished_calls_, 1);

  // No more tasks should remain.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

TEST_F(SerialWorkerTest, CancelDuringWork) {
  worker_->WorkNow();

  RunUntilBreak("OnWork");

  worker_->Cancel();
  UnblockWork();

  RunUntilIdle();
  EXPECT_EQ(breakpoint_, "OnWork");

  EXPECT_EQ(work_finished_calls_, 0);

  // No more tasks should remain.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

TEST_F(SerialWorkerTest, CancelDuringFollowup) {
  followup_immediately_ = false;
  worker_->WorkNow();

  RunUntilBreak("OnWork");
  UnblockWork();
  RunUntilBreak("OnFollowup");

  worker_->Cancel();
  CompleteFollowup();

  RunUntilIdle();
  EXPECT_EQ(breakpoint_, "OnFollowup");

  EXPECT_EQ(work_finished_calls_, 0);

  // No more tasks should remain.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

TEST_F(SerialWorkerTest, DeleteDuringWork) {
  worker_->WorkNow();

  RunUntilBreak("OnWork");

  worker_.reset();
  UnblockWork();

  RunUntilIdle();
  EXPECT_EQ(breakpoint_, "OnWork");

  EXPECT_EQ(work_finished_calls_, 0);

  // No more tasks should remain.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

TEST_F(SerialWorkerTest, DeleteDuringFollowup) {
  followup_immediately_ = false;
  worker_->WorkNow();

  RunUntilBreak("OnWork");
  UnblockWork();
  RunUntilBreak("OnFollowup");

  worker_.reset();
  CompleteFollowup();

  RunUntilIdle();
  EXPECT_EQ(breakpoint_, "OnFollowup");

  EXPECT_EQ(work_finished_calls_, 0);

  // No more tasks should remain.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

TEST_F(SerialWorkerTest, RetryAndThenSucceed) {
  ASSERT_EQ(0, worker_->GetBackoffEntryForTesting().failure_count());

  // Induce a failure.
  on_work_finished_should_report_success_ = false;
  ++input_value_;
  worker_->WorkNow();
  RunUntilBreak("OnWork");
  UnblockWork();
  RunUntilBreak("OnFollowup");
  RunUntilBreak("OnWorkFinished");

  // Confirm it failed and that a retry was scheduled.
  ASSERT_EQ(1, worker_->GetBackoffEntryForTesting().failure_count());
  EXPECT_EQ(kBackoffInitialDelay,
            worker_->GetBackoffEntryForTesting().GetTimeUntilRelease());

  // Make the subsequent attempt succeed.
  on_work_finished_should_report_success_ = true;

  RunUntilBreak("OnWork");
  UnblockWork();
  RunUntilBreak("OnFollowup");
  RunUntilBreak("OnWorkFinished");
  ASSERT_EQ(0, worker_->GetBackoffEntryForTesting().failure_count());

  EXPECT_EQ(2, num_work_calls_observed_);

  // No more tasks should remain.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

TEST_F(SerialWorkerTest, ExternalWorkRequestResetsRetryState) {
  ASSERT_EQ(0, worker_->GetBackoffEntryForTesting().failure_count());

  // Induce a failure.
  on_work_finished_should_report_success_ = false;
  ++input_value_;
  worker_->WorkNow();
  RunUntilBreak("OnWork");
  UnblockWork();
  RunUntilBreak("OnFollowup");
  RunUntilBreak("OnWorkFinished");

  // Confirm it failed and that a retry was scheduled.
  ASSERT_EQ(1, worker_->GetBackoffEntryForTesting().failure_count());
  EXPECT_TRUE(worker_->GetRetryTimerForTesting().IsRunning());
  EXPECT_EQ(kBackoffInitialDelay,
            worker_->GetBackoffEntryForTesting().GetTimeUntilRelease());
  on_work_finished_should_report_success_ = true;

  // The retry state should be reset before we see OnWorkFinished.
  worker_->WorkNow();
  ASSERT_EQ(0, worker_->GetBackoffEntryForTesting().failure_count());
  EXPECT_FALSE(worker_->GetRetryTimerForTesting().IsRunning());
  EXPECT_EQ(base::TimeDelta(),
            worker_->GetBackoffEntryForTesting().GetTimeUntilRelease());
  RunUntilBreak("OnWork");
  UnblockWork();
  RunUntilBreak("OnFollowup");
  RunUntilBreak("OnWorkFinished");

  // No more tasks should remain.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

TEST_F(SerialWorkerTest, MultipleFailureExponentialBackoff) {
  ASSERT_EQ(0, worker_->GetBackoffEntryForTesting().failure_count());

  // Induce a failure.
  on_work_finished_should_report_success_ = false;
  ++input_value_;
  worker_->WorkNow();
  RunUntilBreak("OnWork");
  UnblockWork();
  RunUntilBreak("OnFollowup");
  RunUntilBreak("OnWorkFinished");

  for (int retry_attempt_count = 1; retry_attempt_count <= kMaxRetries;
       retry_attempt_count++) {
    // Confirm it failed and that a retry was scheduled.
    ASSERT_EQ(retry_attempt_count,
              worker_->GetBackoffEntryForTesting().failure_count());
    EXPECT_TRUE(worker_->GetRetryTimerForTesting().IsRunning());
    base::TimeDelta expected_backoff_delay;
    if (retry_attempt_count == 1) {
      expected_backoff_delay = kBackoffInitialDelay;
    } else {
      expected_backoff_delay = kBackoffInitialDelay * kBackoffMultiplyFactor *
                               (retry_attempt_count - 1);
    }
    EXPECT_EQ(expected_backoff_delay,
              worker_->GetBackoffEntryForTesting().GetTimeUntilRelease())
        << "retry_attempt_count=" << retry_attempt_count;

    // |on_work_finished_should_report_success_| is still false, so the retry
    // will fail too
    RunUntilBreak("OnWork");
    UnblockWork();
    RunUntilBreak("OnFollowup");
    RunUntilBreak("OnWorkFinished");
  }

  // The last retry attempt resets the retry state.
  ASSERT_EQ(0, worker_->GetBackoffEntryForTesting().failure_count());
  EXPECT_FALSE(worker_->GetRetryTimerForTesting().IsRunning());
  EXPECT_EQ(base::TimeDelta(),
            worker_->GetBackoffEntryForTesting().GetTimeUntilRelease());
  on_work_finished_should_report_success_ = true;

  // No more tasks should remain.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

}  // namespace

}  // namespace net
