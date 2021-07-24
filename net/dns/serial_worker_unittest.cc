// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/serial_worker.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {
constexpr base::TimeDelta kBackoffInitialDelay =
    base::TimeDelta::FromMilliseconds(100);
constexpr int kBackoffMultiplyFactor = 2;
constexpr int kMaxRetries = 3;

static const BackoffEntry::Policy kTestBackoffPolicy = {
    0,  // Number of initial errors to ignore without backoff.
    static_cast<int>(
        kBackoffInitialDelay
            .InMilliseconds()),  // Initial delay for backoff in ms.
    kBackoffMultiplyFactor,      // Factor to multiply for exponential backoff.
    0,                           // Fuzzing percentage.
    static_cast<int>(base::TimeDelta::FromSeconds(1)
                         .InMilliseconds()),  // Maximum time to delay requests
                                              // in ms: 1 second.
    -1,                                       // Don't discard entry.
    false  // Don't use initial delay unless the last was an error.
};

class SerialWorkerTest : public TestWithTaskEnvironment {
 public:
  // The class under test
  class TestSerialWorker : public SerialWorker {
   public:
    explicit TestSerialWorker(SerialWorkerTest* t)
        : SerialWorker(/*max_number_of_retries=*/kMaxRetries,
                       &kTestBackoffPolicy),
          test_(t) {}
    void DoWork() override {
      CHECK(test_);
      test_->OnWork();
    }
    bool OnWorkFinished() override {
      CHECK(test_);
      return test_->OnWorkFinished();
    }

   private:
    ~TestSerialWorker() override = default;
    SerialWorkerTest* test_;
  };

  // Mocks

  void OnWork() {
    { // Check that OnWork is executed serially.
      base::AutoLock lock(work_lock_);
      EXPECT_FALSE(work_running_) << "DoRead is not called serially!";
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

  bool OnWorkFinished() {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    EXPECT_EQ(output_value_, input_value_);
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

  SerialWorkerTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        work_allowed_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
        work_called_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                     base::WaitableEvent::InitialState::NOT_SIGNALED),
        work_running_(false) {}

  // Helpers for tests.

  // Lets OnWork run and waits for it to complete. Can only return if OnWork is
  // executed on a concurrent thread.
  void WaitForWork() {
    RunUntilBreak("OnWork");
    work_allowed_.Signal();
    work_called_.Wait();
  }

  // test::Test methods
  void SetUp() override {
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
    worker_ = new TestSerialWorker(this);
  }

  void TearDown() override {
    // Cancel the worker to catch if it makes a late DoWork call.
    worker_->Cancel();
    // Check if OnWork is stalled.
    EXPECT_FALSE(work_running_) << "OnWork should be done by TearDown";
    // Release it for cleanliness.
    if (work_running_) {
      WaitForWork();
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
  bool work_running_;
  base::Lock work_lock_;

  // Task runner for this thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // WatcherDelegate under test.
  scoped_refptr<TestSerialWorker> worker_;

  std::string breakpoint_;
  base::RunLoop* run_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SerialWorkerTest);
};

TEST_F(SerialWorkerTest, ExecuteAndSerializeReads) {
  for (int i = 0; i < 3; ++i) {
    ++input_value_;
    worker_->WorkNow();
    WaitForWork();
    RunUntilBreak("OnWorkFinished");

    EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
  }

  // Schedule two calls. OnWork checks if it is called serially.
  ++input_value_;
  worker_->WorkNow();
  // read is blocked, so this will have to induce re-work
  worker_->WorkNow();
  WaitForWork();
  WaitForWork();
  RunUntilBreak("OnWorkFinished");

  // No more tasks should remain.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

TEST_F(SerialWorkerTest, RetryAndThenSucceed) {
  ASSERT_EQ(0, worker_->GetBackoffEntryForTesting().failure_count());

  // Induce a failure.
  on_work_finished_should_report_success_ = false;
  ++input_value_;
  worker_->WorkNow();
  WaitForWork();
  RunUntilBreak("OnWorkFinished");

  // Confirm it failed and that a retry was scheduled.
  ASSERT_EQ(1, worker_->GetBackoffEntryForTesting().failure_count());
  EXPECT_EQ(kBackoffInitialDelay,
            worker_->GetBackoffEntryForTesting().GetTimeUntilRelease());

  // Make the subsequent attempt succeed.
  on_work_finished_should_report_success_ = true;

  WaitForWork();
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
  WaitForWork();
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
  WaitForWork();
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
  WaitForWork();
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
    WaitForWork();
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
