// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_utils.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

// Regression test for crbug.com/1035189.
TEST(ContentTestUtils, NestedRunAllTasksUntilIdleWithPendingThreadPoolWork) {
  base::test::TaskEnvironment task_environment;

  bool thread_pool_task_completed = false;
  base::ThreadPool::PostTask(
      FROM_HERE, {}, base::BindLambdaForTesting([&]() {
        base::PlatformThread::Sleep(base::Milliseconds(100));
        thread_pool_task_completed = true;
      }));

  base::RunLoop run_loop;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Nested RunAllTasksUntilIdle() (i.e. crbug.com/1035189).
        content::RunAllTasksUntilIdle();
        EXPECT_TRUE(thread_pool_task_completed);
        run_loop.Quit();
      }));

  run_loop.Run();
  EXPECT_TRUE(thread_pool_task_completed);
}

// Regression test for crbug.com/1035604.
TEST(ContentTestUtils, FlushRealIOThread) {
  content::BrowserTaskEnvironment task_environment{
      content::BrowserTaskEnvironment::REAL_IO_THREAD};

  bool io_task_completed = false;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        base::PlatformThread::Sleep(base::Milliseconds(100));
        io_task_completed = true;
      }));

  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  EXPECT_TRUE(io_task_completed);
}

TEST(ContentTestUtils, NestedFlushRealIOThread) {
  content::BrowserTaskEnvironment task_environment{
      content::BrowserTaskEnvironment::REAL_IO_THREAD};

  bool io_task_completed = false;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        base::PlatformThread::Sleep(base::Milliseconds(100));
        io_task_completed = true;
      }));

  base::RunLoop run_loop;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
        EXPECT_TRUE(io_task_completed);
        run_loop.Quit();
      }));

  run_loop.Run();
  EXPECT_TRUE(io_task_completed);
}

TEST(ContentTestUtils, FlushRealIOThreadWithPendingBestEffortTask) {
  content::BrowserTaskEnvironment task_environment{
      content::BrowserTaskEnvironment::REAL_IO_THREAD};

  bool io_task_completed = false;
  content::GetIOThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                   base::PlatformThread::Sleep(base::Milliseconds(100));
                   io_task_completed = true;
                 }));

  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  EXPECT_TRUE(io_task_completed);
}

// Same as FlushRealIOThreadWithPendingBestEffortTask but when BrowserThread::IO
// is multiplexed with BrowserThread::UI on the main thread (i.e. the default
// under BrowserTaskEnvironment).
TEST(ContentTestUtils, FlushFakeIOThread) {
  content::BrowserTaskEnvironment task_environment;

  bool io_task_completed = false;
  content::GetIOThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                   base::PlatformThread::Sleep(base::Milliseconds(100));
                   io_task_completed = true;
                 }));

  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  EXPECT_TRUE(io_task_completed);
}

TEST(ContentTestUtils, FlushUIThread) {
  content::BrowserTaskEnvironment task_environment;

  bool ui_task_completed = false;
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                   base::PlatformThread::Sleep(base::Milliseconds(100));
                   ui_task_completed = true;
                 }));

  content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
  EXPECT_TRUE(ui_task_completed);
}
