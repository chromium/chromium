// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/watcher.h"

#include "base/location.h"
#include "base/pending_task.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/scheduler/responsiveness/calculator.h"
#include "content/browser/scheduler/responsiveness/native_event_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace responsiveness {

namespace {

class FakeCalculator : public Calculator {
 public:
  void TaskOrEventFinishedOnUIThread(base::TimeTicks schedule_time,
                                     base::TimeTicks finish_time) override {
    queue_times_ui_.push_back(schedule_time);
  }

  void TaskOrEventFinishedOnIOThread(base::TimeTicks schedule_time,
                                     base::TimeTicks finish_time) override {
    base::AutoLock l(io_thread_lock_);
    queue_times_io_.push_back(schedule_time);
  }

  int NumTasksOnUIThread() { return static_cast<int>(queue_times_ui_.size()); }
  std::vector<base::TimeTicks>& QueueTimesUIThread() { return queue_times_ui_; }
  int NumTasksOnIOThread() {
    base::AutoLock l(io_thread_lock_);
    return static_cast<int>(queue_times_io_.size());
  }
  std::vector<base::TimeTicks>& QueueTimesIOThread() {
    base::AutoLock l(io_thread_lock_);
    return queue_times_io_;
  }

 private:
  std::vector<base::TimeTicks> queue_times_ui_;
  base::Lock io_thread_lock_;
  std::vector<base::TimeTicks> queue_times_io_;
};

class FakeWatcher : public Watcher {
 public:
  std::unique_ptr<Calculator> CreateCalculator() override {
    std::unique_ptr<FakeCalculator> calculator =
        std::make_unique<FakeCalculator>();
    calculator_ = calculator.get();
    return calculator;
  }

  std::unique_ptr<NativeEventObserver> CreateNativeEventObserver() override {
    return nullptr;
  }

  void RegisterMessageLoopObserverUI() override {
    if (register_message_loop_observer_)
      Watcher::RegisterMessageLoopObserverUI();
  }
  void RegisterMessageLoopObserverIO() override {
    if (register_message_loop_observer_)
      Watcher::RegisterMessageLoopObserverIO();
  }

  FakeWatcher(bool register_message_loop_observer)
      : Watcher(),
        register_message_loop_observer_(register_message_loop_observer) {}

  int NumTasksOnUIThread() { return calculator_->NumTasksOnUIThread(); }
  std::vector<base::TimeTicks>& QueueTimesUIThread() {
    return calculator_->QueueTimesUIThread();
  }
  std::vector<base::TimeTicks>& QueueTimesIOThread() {
    return calculator_->QueueTimesIOThread();
  }
  int NumTasksOnIOThread() { return calculator_->NumTasksOnIOThread(); }

 private:
  ~FakeWatcher() override{};
  FakeCalculator* calculator_ = nullptr;
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
    test_browser_thread_bundle_.RunUntilIdle();
  }

  void TearDown() override {
    watcher_->Destroy();
    watcher_.reset();
  }

 protected:
  // This member sets up BrowserThread::IO and BrowserThread::UI. It must be the
  // first member, as other members may depend on these abstractions.
  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  scoped_refptr<FakeWatcher> watcher_;
};

// Test that tasks are forwarded to calculator.
TEST_F(ResponsivenessWatcherTest, TaskForwarding) {
  for (int i = 0; i < 3; ++i) {
    base::PendingTask task(FROM_HERE, base::OnceClosure());
    task.queue_time = base::TimeTicks::Now();
    watcher_->WillRunTaskOnUIThread(&task);
    watcher_->DidRunTaskOnUIThread(&task);
  }
  EXPECT_EQ(3, watcher_->NumTasksOnUIThread());
  EXPECT_EQ(0, watcher_->NumTasksOnIOThread());

  for (int i = 0; i < 4; ++i) {
    base::PendingTask task(FROM_HERE, base::OnceClosure());
    task.queue_time = base::TimeTicks::Now();
    watcher_->WillRunTaskOnIOThread(&task);
    watcher_->DidRunTaskOnIOThread(&task);
  }
  EXPECT_EQ(3, watcher_->NumTasksOnUIThread());
  EXPECT_EQ(4, watcher_->NumTasksOnIOThread());
}

// Test that nested tasks are not forwarded to the calculator.
TEST_F(ResponsivenessWatcherTest, TaskNesting) {
  base::TimeTicks now = base::TimeTicks::Now();

  base::PendingTask task1(FROM_HERE, base::OnceClosure());
  task1.queue_time = now + base::TimeDelta::FromMilliseconds(1);
  base::PendingTask task2(FROM_HERE, base::OnceClosure());
  task2.queue_time = now + base::TimeDelta::FromMilliseconds(2);
  base::PendingTask task3(FROM_HERE, base::OnceClosure());
  task3.queue_time = now + base::TimeDelta::FromMilliseconds(3);

  watcher_->WillRunTaskOnUIThread(&task1);
  watcher_->WillRunTaskOnUIThread(&task2);
  watcher_->WillRunTaskOnUIThread(&task3);
  watcher_->DidRunTaskOnUIThread(&task3);
  watcher_->DidRunTaskOnUIThread(&task2);
  watcher_->DidRunTaskOnUIThread(&task1);

  ASSERT_EQ(1, watcher_->NumTasksOnUIThread());

  // The innermost task should be the one that is passed through, as it didn't
  // cause reentrancy.
  EXPECT_EQ(now + base::TimeDelta::FromMilliseconds(3),
            watcher_->QueueTimesUIThread()[0]);
  EXPECT_EQ(0, watcher_->NumTasksOnIOThread());
}

// Test that native events use execution time instead of queue + execution time.
TEST_F(ResponsivenessWatcherTest, NativeEvents) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  void* opaque_identifier = reinterpret_cast<void*>(0x1234);
  watcher_->WillRunEventOnUIThread(opaque_identifier);
  watcher_->DidRunEventOnUIThread(opaque_identifier);

  ASSERT_EQ(1, watcher_->NumTasksOnUIThread());

  // The queue time should be after |start_time|, since we actually measure
  // execution time rather than queue time + execution time for native events.
  EXPECT_GE(watcher_->QueueTimesUIThread()[0], start_time);
  EXPECT_EQ(0, watcher_->NumTasksOnIOThread());
}

class ResponsivenessWatcherRealIOThreadTest : public testing::Test {
 public:
  ResponsivenessWatcherRealIOThreadTest()
      : test_browser_thread_bundle_(
            content::TestBrowserThreadBundle::REAL_IO_THREAD) {}

  void SetUp() override {
    // Watcher's constructor posts a task to IO thread. We need to let those
    // tasks finish.
    watcher_ = scoped_refptr<FakeWatcher>(
        new FakeWatcher(/*register_message_loop_observer=*/true));
    watcher_->SetUp();
    test_browser_thread_bundle_.RunIOThreadUntilIdle();
  }

  void TearDown() override {
    watcher_->Destroy();
    watcher_.reset();

    // Destroy a task onto the IO thread, which posts back to the UI thread
    // to complete destruction.
    test_browser_thread_bundle_.RunIOThreadUntilIdle();
    test_browser_thread_bundle_.RunUntilIdle();
  }

 protected:
  // This member sets up BrowserThread::IO and BrowserThread::UI. It must be the
  // first member, as other members may depend on these abstractions.
  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  scoped_refptr<FakeWatcher> watcher_;
};

TEST_F(ResponsivenessWatcherRealIOThreadTest, MessageLoopObserver) {
  // Post a do-nothing task onto the UI thread.
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindOnce([]() {}));

  // Post a do-nothing task onto the IO thread.
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                           base::BindOnce([]() {}));

  // Post a task onto the IO thread that hops back to the UI thread. This
  // guarantees that both of the do-nothing tasks have already been processed.
  base::RunLoop run_loop;
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                           base::BindOnce(
                               [](base::OnceClosure quit_closure) {
                                 base::PostTaskWithTraits(
                                     FROM_HERE, {content::BrowserThread::UI},
                                     std::move(quit_closure));
                               },
                               run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_GE(watcher_->NumTasksOnUIThread(), 1);
  EXPECT_FALSE(watcher_->QueueTimesUIThread()[0].is_null());
  ASSERT_GE(watcher_->NumTasksOnIOThread(), 1);
  EXPECT_FALSE(watcher_->QueueTimesIOThread()[0].is_null());
}

}  // namespace responsiveness
}  // namespace content
