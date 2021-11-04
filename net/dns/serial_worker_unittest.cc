// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/serial_worker.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

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

    explicit TestSerialWorker(SerialWorkerTest* t) : test_(t) {}
    ~TestSerialWorker() override = default;

    std::unique_ptr<SerialWorker::WorkItem> CreateWorkItem() override {
      return std::make_unique<TestWorkItem>(test_);
    }

    void OnWorkFinished(
        std::unique_ptr<SerialWorker::WorkItem> work_item) override {
      ASSERT_TRUE(test_);
      test_->OnWorkFinished();
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

  void OnWorkFinished() {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    EXPECT_EQ(output_value_, input_value_);
    ++work_finished_calls_;
    BreakNow("OnWorkFinished");
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
      : input_value_(0),
        output_value_(-1),
        work_allowed_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
        work_called_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                     base::WaitableEvent::InitialState::NOT_SIGNALED),
        work_running_(false) {}

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
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
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
  int input_value_;
  // Output value written on WorkerPool.
  int output_value_;

  // read is called on WorkerPool so we need to synchronize with it.
  base::WaitableEvent work_allowed_;
  base::WaitableEvent work_called_;

  // Protected by read_lock_. Used to verify that read calls are serialized.
  bool work_running_;
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

  base::RunLoop().RunUntilIdle();
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

  base::RunLoop().RunUntilIdle();
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

  base::RunLoop().RunUntilIdle();
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

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(breakpoint_, "OnFollowup");

  EXPECT_EQ(work_finished_calls_, 0);

  // No more tasks should remain.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

}  // namespace

}  // namespace net
