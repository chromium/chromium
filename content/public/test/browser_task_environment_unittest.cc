// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_task_environment.h"

#include <string>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/dcheck_is_on.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::TaskEnvironment;

namespace content {

namespace {

// BrowserTaskEnvironmentTest.RunUntilIdle will run kNumTasks tasks that will
// hop back-and-forth between ThreadPool and UI thread kNumHops times.
// Note: These values are arbitrary.
constexpr int kNumHops = 13;
constexpr int kNumTasks = 8;

void PostTaskToUIThread(int iteration, base::subtle::Atomic32* tasks_run);

void PostToThreadPool(int iteration, base::subtle::Atomic32* tasks_run) {
  // All iterations but the first come from a task that was posted.
  if (iteration > 0)
    base::subtle::NoBarrier_AtomicIncrement(tasks_run, 1);

  if (iteration == kNumHops)
    return;

  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(&PostTaskToUIThread, iteration + 1, tasks_run));
}

void PostTaskToUIThread(int iteration, base::subtle::Atomic32* tasks_run) {
  // All iterations but the first come from a task that was posted.
  if (iteration > 0)
    base::subtle::NoBarrier_AtomicIncrement(tasks_run, 1);

  if (iteration == kNumHops)
    return;

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PostToThreadPool, iteration + 1, tasks_run));
}

}  // namespace

TEST(BrowserTaskEnvironmentTest, RunUntilIdle) {
  BrowserTaskEnvironment task_environment;

  base::subtle::Atomic32 tasks_run = 0;

  // Post half the tasks on ThreadPool and the other half on the UI thread
  // so they cross and the last hops aren't all on the same task runner.
  for (int i = 0; i < kNumTasks; ++i) {
    if (i % 2) {
      PostToThreadPool(0, &tasks_run);
    } else {
      PostTaskToUIThread(0, &tasks_run);
    }
  }

  task_environment.RunUntilIdle();

  EXPECT_EQ(kNumTasks * kNumHops, base::subtle::NoBarrier_Load(&tasks_run));
}

namespace {

void PostRecurringTaskToIOThread(int iteration, int* tasks_run) {
  // All iterations but the first come from a task that was posted.
  if (iteration > 0)
    (*tasks_run)++;

  if (iteration == kNumHops)
    return;

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PostRecurringTaskToIOThread, iteration + 1, tasks_run));
}

}  // namespace

TEST(BrowserTaskEnvironmentTest, RunIOThreadUntilIdle) {
  BrowserTaskEnvironment task_environment(
      BrowserTaskEnvironment::Options::REAL_IO_THREAD);

  int tasks_run = 0;

  for (int i = 0; i < kNumTasks; ++i) {
    PostRecurringTaskToIOThread(0, &tasks_run);
  }

  task_environment.RunIOThreadUntilIdle();

  EXPECT_EQ(kNumTasks * kNumHops, tasks_run);
}

TEST(BrowserTaskEnvironmentTest, MessageLoopTypeMismatch) {
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);

  EXPECT_DEATH_IF_SUPPORTED(
      {
        BrowserTaskEnvironment task_environment(
            BrowserTaskEnvironment::IO_MAINLOOP);
      },
      "");
}

TEST(BrowserTaskEnvironmentTest, MultipleBrowserTaskEnvironment) {
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  EXPECT_DEATH_IF_SUPPORTED(
      {
        BrowserTaskEnvironment task_environment;
        BrowserTaskEnvironment other_task_environment;
      },
      "");
}

TEST(BrowserTaskEnvironmentTest, TraitsConstructor) {
  BrowserTaskEnvironment task_environment(
      BrowserTaskEnvironment::Options::REAL_IO_THREAD,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED);
  // Should set up a UI main thread.
  EXPECT_TRUE(base::CurrentUIThread::IsSet());
  EXPECT_FALSE(base::CurrentIOThread::IsSet());

  // Should create a real IO thread. If it was on the same thread the following
  // will timeout.
  base::WaitableEvent signaled_on_real_io_thread;
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                Unretained(&signaled_on_real_io_thread)));
  signaled_on_real_io_thread.TimedWait(base::TimeDelta::FromSeconds(5));
  EXPECT_TRUE(signaled_on_real_io_thread.IsSignaled());

  // Tasks posted via ThreadPool::PostTask don't run in
  // ThreadPoolExecutionMode::QUEUED until RunUntilIdle is called.
  base::AtomicFlag task_ran;
  base::ThreadPool::PostTask(
      FROM_HERE, BindOnce([](base::AtomicFlag* task_ran) { task_ran->Set(); },
                          Unretained(&task_ran)));

  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(task_ran.IsSet());

  task_environment.RunUntilIdle();
  EXPECT_TRUE(task_ran.IsSet());
}

TEST(BrowserTaskEnvironmentTest, TraitsConstructorOverrideMainThreadType) {
  BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  // Should set up a UI main thread.
  EXPECT_TRUE(base::CurrentUIThread::IsSet());
  EXPECT_FALSE(base::CurrentIOThread::IsSet());

  // There should be a mock clock.
  EXPECT_THAT(task_environment.GetMockClock(), testing::NotNull());
}

// Verify that posting tasks to the UI/IO threads without having the
// BrowserTaskEnvironment instance causes a crash.
TEST(BrowserTaskEnvironmentTest, NotInitialized) {
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);

  std::string death_matcher;
#if DCHECK_IS_ON() && !defined(OS_ANDROID)
  // Expect that in builds with working DCHECK messages the failure message
  // includes a hint towards using the BrowserTaskEnvironment class.
  death_matcher = "Check failed:.*\n*.*BrowserTaskEnvironment";
#endif

  EXPECT_DEATH_IF_SUPPORTED(
      { GetUIThreadTaskRunner({})->PostTask(FROM_HERE, base::DoNothing()); },
      death_matcher);
  EXPECT_DEATH_IF_SUPPORTED(
      { GetIOThreadTaskRunner({})->PostTask(FROM_HERE, base::DoNothing()); },
      death_matcher);

  EXPECT_DEATH_IF_SUPPORTED(
      { base::PostTask(FROM_HERE, {BrowserThread::UI}, base::DoNothing()); },
      death_matcher);
  EXPECT_DEATH_IF_SUPPORTED(
      { base::PostTask(FROM_HERE, {BrowserThread::IO}, base::DoNothing()); },
      death_matcher);
}

}  // namespace content
