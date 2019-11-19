// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/serial_worker.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
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
    explicit TestSerialWorker(SerialWorkerTest* t) : test_(t) {}
    void DoWork() override {
      ASSERT_TRUE(test_);
      test_->OnWork();
    }
    void OnWorkFinished() override {
      ASSERT_TRUE(test_);
      test_->OnWorkFinished();
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

  void OnWorkFinished() {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    EXPECT_EQ(output_value_, input_value_);
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
  int input_value_;
  // Output value written on WorkerPool.
  int output_value_;

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

    EXPECT_TRUE(base::MessageLoopCurrent::Get()->IsIdleForTesting());
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
  EXPECT_TRUE(base::MessageLoopCurrent::Get()->IsIdleForTesting());
}

}  // namespace

}  // namespace net
