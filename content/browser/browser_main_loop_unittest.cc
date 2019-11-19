// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main_loop.h"

#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_command_line.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/browser/startup_data_impl.h"
#include "content/browser/startup_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using StrickMockTask =
    testing::StrictMock<base::MockCallback<base::RepeatingCallback<void()>>>;

class BrowserMainLoopTest : public testing::Test {
 protected:
  BrowserMainLoopTest() {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        switches::kSingleProcess);
    base::ThreadPoolInstance::Create("Browser");
    StartBrowserThreadPool();
    BrowserTaskExecutor::Create();
  }

  ~BrowserMainLoopTest() override {
    BrowserTaskExecutor::ResetForTesting();
    for (int id = BrowserThread::UI; id < BrowserThread::ID_COUNT; ++id) {
      BrowserThreadImpl::ResetGlobalsForTesting(
          static_cast<BrowserThread::ID>(id));
    }
    base::ThreadPoolInstance::Get()->JoinForTesting();
    base::ThreadPoolInstance::Set(nullptr);
  }

  const base::CommandLine& GetProcessCommandLine() {
    return *scoped_command_line_.GetProcessCommandLine();
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
};  // namespace content

// Verify that a single-process browser process has at least as many threads as
// the number of cores in its foreground pool.
TEST_F(BrowserMainLoopTest, CreateThreadsInSingleProcess) {
  MainFunctionParams main_function_params(GetProcessCommandLine());

  StartupDataImpl startup_data;
  startup_data.ipc_thread = BrowserTaskExecutor::CreateIOThread();
  main_function_params.startup_data = &startup_data;

  BrowserMainLoop browser_main_loop(
      main_function_params,
      std::make_unique<base::ThreadPoolInstance::ScopedExecutionFence>());
  browser_main_loop.MainMessageLoopStart();
  browser_main_loop.Init();
  browser_main_loop.CreateThreads();
  EXPECT_GE(base::ThreadPoolInstance::Get()
                ->GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                    {base::ThreadPool(), base::TaskPriority::USER_VISIBLE}),
            base::SysInfo::NumberOfProcessors() - 1);
  browser_main_loop.ShutdownThreadsAndCleanUp();
  BrowserTaskExecutor::ResetForTesting();
}

TEST_F(BrowserMainLoopTest,
       PostTaskToIOThreadBeforeThreadCreationDoesNotRunTask) {
  MainFunctionParams main_function_params(GetProcessCommandLine());

  StartupDataImpl startup_data;
  startup_data.ipc_thread = BrowserTaskExecutor::CreateIOThread();
  main_function_params.startup_data = &startup_data;

  BrowserMainLoop browser_main_loop(
      main_function_params,
      std::make_unique<base::ThreadPoolInstance::ScopedExecutionFence>());
  browser_main_loop.MainMessageLoopStart();
  browser_main_loop.Init();

  StrickMockTask task;

  // No task should run because IO thread has not been initialized yet.
  base::PostTask(FROM_HERE, {BrowserThread::IO}, task.Get());
  base::CreateTaskRunner({BrowserThread::IO})->PostTask(FROM_HERE, task.Get());

  content::RunAllPendingInMessageLoop(BrowserThread::IO);

  EXPECT_CALL(task, Run).Times(2);
  browser_main_loop.CreateThreads();
  content::RunAllPendingInMessageLoop(BrowserThread::IO);

  browser_main_loop.ShutdownThreadsAndCleanUp();
  BrowserTaskExecutor::ResetForTesting();
}

}  // namespace content
