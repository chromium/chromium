// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main_loop.h"

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/system/sys_info.h"
#include "base/task/execution_fence.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_command_line.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/browser/startup_data_impl.h"
#include "content/browser/startup_helper.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/test_utils.h"
#include "services/tracing/perfetto/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

#if !BUILDFLAG(IS_ANDROID)

namespace {

// A BrowserMainParts that aborts startup from PostCreateThreads(), mimicking
// ChromeBrowserMainPartsWin when upgrade_util::DoUpgradeTasks() has swapped in
// a newer executable and relaunched the browser.
class AbortingBrowserMainParts : public BrowserMainParts {
 public:
  AbortingBrowserMainParts(bool* post_create_threads_called,
                           bool* pre_main_message_loop_run_called)
      : post_create_threads_called_(post_create_threads_called),
        pre_main_message_loop_run_called_(pre_main_message_loop_run_called) {}

  int PostCreateThreads() override {
    *post_create_threads_called_ = true;
    return RESULT_CODE_KILLED;
  }

  int PreMainMessageLoopRun() override {
    *pre_main_message_loop_run_called_ = true;
    return RESULT_CODE_NORMAL_EXIT;
  }

 private:
  raw_ptr<bool> post_create_threads_called_;
  raw_ptr<bool> pre_main_message_loop_run_called_;
};

class AbortingContentBrowserClient : public ContentBrowserClient {
 public:
  AbortingContentBrowserClient(bool* post_create_threads_called,
                               bool* pre_main_message_loop_run_called)
      : post_create_threads_called_(post_create_threads_called),
        pre_main_message_loop_run_called_(pre_main_message_loop_run_called) {}

  std::unique_ptr<BrowserMainParts> CreateBrowserMainParts(
      bool /*is_integration_test*/) override {
    return std::make_unique<AbortingBrowserMainParts>(
        post_create_threads_called_, pre_main_message_loop_run_called_);
  }

 private:
  raw_ptr<bool> post_create_threads_called_;
  raw_ptr<bool> pre_main_message_loop_run_called_;
};

}  // namespace

#endif  // !BUILDFLAG(IS_ANDROID)

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

  const base::CommandLine* GetProcessCommandLine() {
    return scoped_command_line_.GetProcessCommandLine();
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
};  // namespace content

// Android runs startup tasks asynchronously and reports completion over JNI, so
// this synchronous test does not apply there.
#if !BUILDFLAG(IS_ANDROID)

// Verify that a non-normal result from BrowserMainParts::PostCreateThreads()
// aborts startup, so that the remaining startup tasks - including the ones that
// launch child processes - never run.
TEST_F(BrowserMainLoopTest, PostCreateThreadsResultAbortsStartup) {
  bool post_create_threads_called = false;
  bool pre_main_message_loop_run_called = false;
  AbortingContentBrowserClient browser_client(
      &post_create_threads_called, &pre_main_message_loop_run_called);
  ScopedContentBrowserClientSetting browser_client_setting(&browser_client);
  MainFunctionParams main_function_params(GetProcessCommandLine());

  BrowserMainLoop browser_main_loop(
      std::move(main_function_params),
      std::make_unique<base::ScopedThreadPoolExecutionFence>());

  // PreCreateThreads() builds the tracing controller, which requires an
  // initialized PerfettoTracedProcess. Host it on a dedicated thread so
  // teardown doesn't depend on the ThreadPool this test shuts down, and declare
  // it after `browser_main_loop` so it is destroyed first: its destructor pumps
  // the main thread, which production never does once startup has aborted.
  base::Thread tracing_thread("TracingTest");
  ASSERT_TRUE(tracing_thread.Start());
  tracing::TracedProcessForTesting traced_process(tracing_thread.task_runner());

  browser_main_loop.Init();
  browser_main_loop.CreateMainMessageLoop();
  browser_main_loop.CreateStartupTasks();

  EXPECT_TRUE(post_create_threads_called);

  // BrowserMainRunnerImpl returns this result from Initialize() and never runs
  // the main message loop.
  EXPECT_EQ(RESULT_CODE_KILLED, browser_main_loop.GetResultCode());
  EXPECT_FALSE(pre_main_message_loop_run_called);

  browser_main_loop.ShutdownThreadsAndCleanUp();
}

#endif  // !BUILDFLAG(IS_ANDROID)

// Verify that a single-process browser process has at least as many threads as
// the number of cores in its foreground pool.
TEST_F(BrowserMainLoopTest, CreateThreadsInSingleProcess) {
  MainFunctionParams main_function_params(GetProcessCommandLine());

  auto startup_data = std::make_unique<StartupDataImpl>();
  startup_data->io_thread = BrowserTaskExecutor::CreateIOThread();
  main_function_params.startup_data = std::move(startup_data);

  BrowserMainLoop browser_main_loop(
      std::move(main_function_params),
      std::make_unique<base::ScopedThreadPoolExecutionFence>());
  browser_main_loop.Init();
  browser_main_loop.CreateMainMessageLoop();
  browser_main_loop.CreateThreads();
  EXPECT_GE(base::ThreadPoolInstance::Get()->GetMaxConcurrentForegroundTasks(),
            static_cast<size_t>(base::SysInfo::NumberOfProcessors() - 1));
  browser_main_loop.ShutdownThreadsAndCleanUp();
  BrowserTaskExecutor::ResetForTesting();
}

TEST_F(BrowserMainLoopTest,
       PostTaskToIOThreadBeforeThreadCreationDoesNotRunTask) {
  MainFunctionParams main_function_params(GetProcessCommandLine());

  auto startup_data = std::make_unique<StartupDataImpl>();
  startup_data->io_thread = BrowserTaskExecutor::CreateIOThread();
  main_function_params.startup_data = std::move(startup_data);

  BrowserMainLoop browser_main_loop(
      std::move(main_function_params),
      std::make_unique<base::ScopedThreadPoolExecutionFence>());
  browser_main_loop.Init();
  browser_main_loop.CreateMainMessageLoop();

  StrickMockTask task;

  // No task should run because IO thread has not been initialized yet.
  GetIOThreadTaskRunner({})->PostTask(FROM_HERE, task.Get());
  GetIOThreadTaskRunner({})->PostTask(FROM_HERE, task.Get());

  content::RunAllPendingInMessageLoop(BrowserThread::IO);

  EXPECT_CALL(task, Run).Times(2);
  browser_main_loop.CreateThreads();
  content::RunAllPendingInMessageLoop(BrowserThread::IO);

  browser_main_loop.ShutdownThreadsAndCleanUp();
  BrowserTaskExecutor::ResetForTesting();
}

}  // namespace content
