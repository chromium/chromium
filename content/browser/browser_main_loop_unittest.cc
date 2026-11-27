// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main_loop.h"

#include <algorithm>
#include <limits>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/system/sys_info.h"
#include "base/task/execution_fence.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/mock_callback.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/browser/startup_data_impl.h"
#include "content/browser/startup_helper.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/test_content_client.h"
#include "content/public/test/test_utils.h"
#include "services/tracing/perfetto/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

#if BUILDFLAG(IS_LINUX)
#include <sys/resource.h>
#endif

#if BUILDFLAG(IS_MAC)
#include "base/test/mock_chrome_application_mac.h"
#endif

namespace content {

namespace {

class ScopedBrowserMainLoopTestEnv {
 public:
  ScopedBrowserMainLoopTestEnv() {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        switches::kSingleProcess);
    base::ThreadPoolInstance::Create("Browser");
    StartBrowserThreadPool();
    BrowserTaskExecutor::Create();
  }

  ~ScopedBrowserMainLoopTestEnv() {
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
  // Reset cached runners after the ThreadPool has been destroyed.
  base::internal::ScopedLazyTaskRunnerListForTesting
      scoped_lazy_task_runner_list_;
  base::test::ScopedCommandLine scoped_command_line_;
};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

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

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace

using StrickMockTask =
    testing::StrictMock<base::MockCallback<base::RepeatingCallback<void()>>>;

class BrowserMainLoopTest : public testing::Test {
 protected:
  const base::CommandLine* GetProcessCommandLine() {
    return test_env_.GetProcessCommandLine();
  }

 private:
  ScopedBrowserMainLoopTestEnv test_env_;
};

// Android runs startup tasks asynchronously and reports completion over JNI.
// iOS stubs out `base::LaunchProcess()` (so multiprocess tests cannot run
// there) and does not use `PostCreateThreads()` to abort startup.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

MULTIPROCESS_TEST_MAIN(PostCreateThreadsResultAbortsStartupChild) {
#if BUILDFLAG(IS_MAC)
  // `ContentTestSuite::Initialize()` skips registering `MockCrApp` in child
  // processes (for sandbox tests), but `BrowserTaskExecutor::Create()` needs an
  // `NSApplication` implementing `CrAppProtocol` for the UI message pump.
  mock_cr_app::RegisterMockCrApp();
#endif

  ScopedBrowserMainLoopTestEnv test_env;

  TestContentClient content_client;
  SetContentClient(&content_client);
  absl::Cleanup reset_content_client = [] { SetContentClient(nullptr); };

  bool post_create_threads_called = false;
  bool pre_main_message_loop_run_called = false;
  AbortingContentBrowserClient browser_client(
      &post_create_threads_called, &pre_main_message_loop_run_called);
  ScopedContentBrowserClientSetting browser_client_setting(&browser_client);
  MainFunctionParams main_function_params(test_env.GetProcessCommandLine());
  auto startup_data = std::make_unique<StartupDataImpl>();
  startup_data->background_tracing_manager =
      std::make_unique<BackgroundTracingManagerImpl>(
          browser_client.CreateTracingDelegate());
  main_function_params.startup_data = std::move(startup_data);

  BrowserMainLoop browser_main_loop(
      std::move(main_function_params),
      std::make_unique<base::ScopedThreadPoolExecutionFence>());

  // PreCreateThreads() builds the tracing controller, which requires an
  // initialized PerfettoTracedProcess. Host it on a dedicated thread so
  // teardown doesn't depend on the ThreadPool this test shuts down, and declare
  // it after `browser_main_loop` so it is destroyed first: its destructor pumps
  // the main thread, which production never does once startup has aborted.
  base::Thread tracing_thread("TracingTest");
  CHECK(tracing_thread.Start());
  tracing::TracedProcessForTesting traced_process(tracing_thread.task_runner());

  browser_main_loop.Init();
  browser_main_loop.CreateMainMessageLoop();
  browser_main_loop.CreateStartupTasks();

  CHECK(post_create_threads_called);

  // BrowserMainRunnerImpl returns this result from Initialize() and never runs
  // the main message loop.
  CHECK_EQ(RESULT_CODE_KILLED, browser_main_loop.GetResultCode());
  CHECK(!pre_main_message_loop_run_called);

  browser_main_loop.ShutdownThreadsAndCleanUp();
  return 0;
}

// Verify that a non-normal result from BrowserMainParts::PostCreateThreads()
// aborts startup, so that the remaining startup tasks - including the ones that
// launch child processes - never run.
//
// Runs in a fresh child process because, unlike the other
// `BrowserMainLoopTest` cases (which call `CreateThreads()` directly), this
// test runs `CreateStartupTasks()` / `PreCreateThreads()`.
// `PreCreateThreads()` asserts and initializes process-wide one-shot
// singletons (such as `GpuDataManagerImpl`, `TracingControllerImpl`, and the
// pseudonymization salt), which would otherwise collide with other tests
// batched in the same `content_unittests` process.
TEST(BrowserMainLoopStartupTest, PostCreateThreadsResultAbortsStartup) {
  base::LaunchOptions options;
#if BUILDFLAG(IS_WIN)
  options.start_hidden = true;
#endif
  base::Process child_process = base::SpawnMultiProcessTestChild(
      "PostCreateThreadsResultAbortsStartupChild",
      base::GetMultiProcessTestChildBaseCommandLine(), options);
  ASSERT_TRUE(child_process.IsValid());

  int exit_code = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(0, exit_code);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

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

#if BUILDFLAG(IS_LINUX)
// On Linux, the soft limit on open file descriptors is raised to the hard
// limit, so that the browser doesn't run out of descriptors with many tabs.
// See https://crbug.com/436013925.
TEST(BrowserMainLoopFdLimitTest, IncreaseFdLimitRaisesSoftLimitToHardLimit) {
  struct rlimit original_limit;
  ASSERT_EQ(0, getrlimit(RLIMIT_NOFILE, &original_limit));

  struct rlimit low_limit = original_limit;
  low_limit.rlim_cur = std::min<rlim_t>(original_limit.rlim_max, 1024);
  ASSERT_EQ(0, setrlimit(RLIMIT_NOFILE, &low_limit));

  BrowserMainLoop::IncreaseFdLimit();

  struct rlimit new_limit;
  ASSERT_EQ(0, getrlimit(RLIMIT_NOFILE, &new_limit));
  ASSERT_EQ(0, setrlimit(RLIMIT_NOFILE, &original_limit));

  EXPECT_EQ(std::min<rlim_t>(original_limit.rlim_max,
                             std::numeric_limits<unsigned int>::max()),
            new_limit.rlim_cur);
  EXPECT_EQ(original_limit.rlim_max, new_limit.rlim_max);
}
#endif  // BUILDFLAG(IS_LINUX)

}  // namespace content
