// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_thread_bundle.h"

#include "base/atomicops.h"
#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "base/test/scoped_task_environment.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::ScopedTaskEnvironment;

namespace content {

TEST(TestBrowserThreadBundleTest,
     ScopedTaskEnvironmentAndTestBrowserThreadBundle) {
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::MainThreadType::UI);
  TestBrowserThreadBundle test_browser_thread_bundle;
  base::PostTaskAndReply(FROM_HERE, base::DoNothing(), base::BindOnce([]() {
                           DCHECK_CURRENTLY_ON(BrowserThread::UI);
                         }));
  scoped_task_environment.RunUntilIdle();
}

// Regression test to verify that ~TestBrowserThreadBundle() doesn't hang when
// the TaskScheduler is owned by a QUEUED ScopedTaskEnvironment with pending
// tasks.
TEST(TestBrowserThreadBundleTest,
     QueuedScopedTaskEnvironmentAndTestBrowserThreadBundle) {
  ScopedTaskEnvironment queued_scoped_task_environment(
      ScopedTaskEnvironment::MainThreadType::UI,
      ScopedTaskEnvironment::ExecutionMode::QUEUED);
  base::PostTask(FROM_HERE, base::DoNothing());

  {
    TestBrowserThreadBundle test_browser_thread_bundle;
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }  // Would hang here prior to fix.
}

namespace {

// TestBrowserThreadBundleTest.RunUntilIdle will run kNumTasks tasks that will
// hop back-and-forth between TaskScheduler and UI thread kNumHops times.
// Note: These values are arbitrary.
constexpr int kNumHops = 13;
constexpr int kNumTasks = 8;

void PostTaskToUIThread(int iteration, base::subtle::Atomic32* tasks_run);

void PostToTaskScheduler(int iteration, base::subtle::Atomic32* tasks_run) {
  // All iterations but the first come from a task that was posted.
  if (iteration > 0)
    base::subtle::NoBarrier_AtomicIncrement(tasks_run, 1);

  if (iteration == kNumHops)
    return;

  base::PostTask(FROM_HERE,
                 base::BindOnce(&PostTaskToUIThread, iteration + 1, tasks_run));
}

void PostTaskToUIThread(int iteration, base::subtle::Atomic32* tasks_run) {
  // All iterations but the first come from a task that was posted.
  if (iteration > 0)
    base::subtle::NoBarrier_AtomicIncrement(tasks_run, 1);

  if (iteration == kNumHops)
    return;

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&PostToTaskScheduler, iteration + 1, tasks_run));
}

}  // namespace

TEST(TestBrowserThreadBundleTest, RunUntilIdle) {
  TestBrowserThreadBundle test_browser_thread_bundle;

  base::subtle::Atomic32 tasks_run = 0;

  // Post half the tasks on TaskScheduler and the other half on the UI thread
  // so they cross and the last hops aren't all on the same task runner.
  for (int i = 0; i < kNumTasks; ++i) {
    if (i % 2) {
      PostToTaskScheduler(0, &tasks_run);
    } else {
      PostTaskToUIThread(0, &tasks_run);
    }
  }

  test_browser_thread_bundle.RunUntilIdle();

  EXPECT_EQ(kNumTasks * kNumHops, base::subtle::NoBarrier_Load(&tasks_run));
}

namespace {

void PostRecurringTaskToIOThread(int iteration, int* tasks_run) {
  // All iterations but the first come from a task that was posted.
  if (iteration > 0)
    (*tasks_run)++;

  if (iteration == kNumHops)
    return;

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&PostRecurringTaskToIOThread, iteration + 1, tasks_run));
}

}  // namespace

TEST(TestBrowserThreadBundleTest, RunIOThreadUntilIdle) {
  TestBrowserThreadBundle test_browser_thread_bundle(
      TestBrowserThreadBundle::Options::REAL_IO_THREAD);

  int tasks_run = 0;

  for (int i = 0; i < kNumTasks; ++i) {
    PostRecurringTaskToIOThread(0, &tasks_run);
  }

  test_browser_thread_bundle.RunIOThreadUntilIdle();

  EXPECT_EQ(kNumTasks * kNumHops, tasks_run);
}

TEST(TestBrowserThreadBundleTest, MessageLoopTypeMismatch) {
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::UI);

  EXPECT_DEATH_IF_SUPPORTED(
      {
        TestBrowserThreadBundle test_browser_thread_bundle(
            TestBrowserThreadBundle::IO_MAINLOOP);
      },
      "");
}

TEST(TestBrowserThreadBundleTest, MultipleTestBrowserThreadBundle) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        TestBrowserThreadBundle test_browser_thread_bundle;
        TestBrowserThreadBundle other_test_browser_thread_bundle;
      },
      "");
}

}  // namespace content
