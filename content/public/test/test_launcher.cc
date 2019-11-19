// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_launcher.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/gtest_xml_util.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/test/browser_test.h"
#include "gpu/config/gpu_switches.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/buildflags.h"
#include "ui/base/ui_base_features.h"

#if defined(OS_POSIX)
#include "base/files/file_descriptor_watcher_posix.h"
#endif

#if defined(OS_WIN)
#include "base/base_switches.h"
#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_types.h"
#include "services/service_manager/sandbox/win/sandbox_win.h"
#elif defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "sandbox/mac/seatbelt_exec.h"
#endif

namespace content {

namespace {

// Tests with this prefix run before the same test without it, and use the same
// profile. i.e. Foo.PRE_Test runs and then Foo.Test. This allows writing tests
// that span browser restarts.
const char kPreTestPrefix[] = "PRE_";

const char kManualTestPrefix[] = "MANUAL_";

TestLauncherDelegate* g_launcher_delegate = nullptr;
#if !defined(OS_ANDROID)
// ContentMain is not run on Android in the test process, and is run via
// java for child processes. So ContentMainParams does not exist there.
ContentMainParams* g_params = nullptr;
#endif

void PrintUsage() {
  fprintf(stdout,
          "Runs tests using the gtest framework, each batch of tests being\n"
          "run in their own process. Supported command-line flags:\n"
          "\n"
          " Common flags:\n"
          "  --gtest_filter=...\n"
          "    Runs a subset of tests (see --gtest_help for more info).\n"
          "\n"
          "  --help\n"
          "    Shows this message.\n"
          "\n"
          "  --gtest_help\n"
          "    Shows the gtest help message.\n"
          "\n"
          "  --test-launcher-jobs=N\n"
          "    Sets the number of parallel test jobs to N.\n"
          "\n"
          "  --single_process\n"
          "    Runs the tests and the launcher in the same process. Useful\n"
          "    for debugging a specific test in a debugger.\n"
          "\n"
          " Other flags:\n"
          "  --test-launcher-retry-limit=N\n"
          "    Sets the limit of test retries on failures to N.\n"
          "\n"
          "  --test-launcher-summary-output=PATH\n"
          "    Saves a JSON machine-readable summary of the run.\n"
          "\n"
          "  --test-launcher-print-test-stdio=auto|always|never\n"
          "    Controls when full test output is printed.\n"
          "    auto means to print it when the test failed.\n"
          "\n"
          "  --test-launcher-total-shards=N\n"
          "    Sets the total number of shards to N.\n"
          "\n"
          "  --test-launcher-shard-index=N\n"
          "    Sets the shard index to run to N (from 0 to TOTAL - 1).\n");
}

// Implementation of base::TestLauncherDelegate. This is also a test launcher,
// wrapping a lower-level test launcher with content-specific code.
class WrapperTestLauncherDelegate : public base::TestLauncherDelegate {
 public:
  explicit WrapperTestLauncherDelegate(
      content::TestLauncherDelegate* launcher_delegate)
      : launcher_delegate_(launcher_delegate) {
    run_manual_tests_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kRunManualTestsFlag);
  }

  // base::TestLauncherDelegate:
  bool GetTests(std::vector<base::TestIdentifier>* output) override;

  base::CommandLine GetCommandLine(const std::vector<std::string>& test_names,
                                   const base::FilePath& temp_dir,
                                   base::FilePath* output_file) override;

  size_t GetBatchSize() override;

  std::string GetWrapper() override;

  int GetLaunchOptions() override;

  base::TimeDelta GetTimeout() override;

  bool ShouldRunTest(const base::TestIdentifier& test) override;

 private:
  // Relays timeout notification from the TestLauncher (by way of a
  // ProcessLifetimeObserver) to the caller's content::TestLauncherDelegate.
  void OnTestTimedOut(const base::CommandLine& command_line) override;

  // Delegate additional TestResult processing.
  void ProcessTestResults(std::vector<base::TestResult>& test_results,
                          base::TimeDelta elapsed_time) override;

  content::TestLauncherDelegate* launcher_delegate_;

  bool run_manual_tests_ = false;

  DISALLOW_COPY_AND_ASSIGN(WrapperTestLauncherDelegate);
};

bool WrapperTestLauncherDelegate::GetTests(
    std::vector<base::TestIdentifier>* output) {
  *output = base::GetCompiledInTests();
  return true;
}

bool IsPreTestName(const std::string& test_name) {
  return test_name.find(kPreTestPrefix) != std::string::npos;
}

size_t WrapperTestLauncherDelegate::GetBatchSize() {
  return 1u;
}

base::CommandLine WrapperTestLauncherDelegate::GetCommandLine(
    const std::vector<std::string>& test_names,
    const base::FilePath& temp_dir,
    base::FilePath* output_file) {
  DCHECK_EQ(1u, test_names.size());
  std::string test_name(test_names.front());
  // Chained pre tests must share the same temp directory,
  // TestLauncher should guarantee that for the delegate.
  base::FilePath user_data_dir = temp_dir.AppendASCII("user_data");
  CreateDirectory(user_data_dir);
  base::CommandLine cmd_line(*base::CommandLine::ForCurrentProcess());
  launcher_delegate_->PreRunTest();
  CHECK(launcher_delegate_->AdjustChildProcessCommandLine(&cmd_line,
                                                          user_data_dir));
  base::CommandLine new_cmd_line(cmd_line.GetProgram());
  base::CommandLine::SwitchMap switches = cmd_line.GetSwitches();
  // Strip out gtest_output flag because otherwise we would overwrite results
  // of the other tests.
  switches.erase(base::kGTestOutputFlag);

  // Create a dedicated temporary directory to store the xml result data
  // per run to ensure clean state and make it possible to launch multiple
  // processes in parallel.
  CHECK(base::CreateTemporaryDirInDir(temp_dir, FILE_PATH_LITERAL("results"),
                                      output_file));
  *output_file = output_file->AppendASCII("test_results.xml");

  new_cmd_line.AppendSwitchPath(switches::kTestLauncherOutput, *output_file);

  for (base::CommandLine::SwitchMap::const_iterator iter = switches.begin();
       iter != switches.end(); ++iter) {
    new_cmd_line.AppendSwitchNative(iter->first, iter->second);
  }

  // Always enable disabled tests.  This method is not called with disabled
  // tests unless this flag was specified to the browser test executable.
  new_cmd_line.AppendSwitch("gtest_also_run_disabled_tests");
  new_cmd_line.AppendSwitchASCII("gtest_filter", test_name);
  new_cmd_line.AppendSwitch(switches::kSingleProcessTests);
  return new_cmd_line;
}

std::string WrapperTestLauncherDelegate::GetWrapper() {
  char* browser_wrapper = getenv("BROWSER_WRAPPER");
  return browser_wrapper ? browser_wrapper : std::string();
}

int WrapperTestLauncherDelegate::GetLaunchOptions() {
  return base::TestLauncher::USE_JOB_OBJECTS |
         base::TestLauncher::ALLOW_BREAKAWAY_FROM_JOB;
}

base::TimeDelta WrapperTestLauncherDelegate::GetTimeout() {
  return TestTimeouts::test_launcher_timeout();
}

void WrapperTestLauncherDelegate::OnTestTimedOut(
    const base::CommandLine& command_line) {
  launcher_delegate_->OnTestTimedOut(command_line);
}

void WrapperTestLauncherDelegate::ProcessTestResults(
    std::vector<base::TestResult>& test_results,
    base::TimeDelta elapsed_time) {
  CHECK_EQ(1u, test_results.size());

  test_results.front().elapsed_time = elapsed_time;

  launcher_delegate_->PostRunTest(&test_results.front());
}
// TODO(isamsonov): crbug.com/1004417 remove when windows builders
// stop flaking on MANAUAL_ tests.
bool WrapperTestLauncherDelegate::ShouldRunTest(
    const base::TestIdentifier& test) {
  return run_manual_tests_ ||
         !base::StartsWith(test.test_name, kManualTestPrefix,
                           base::CompareCase::SENSITIVE);
}

}  // namespace

void AppendCommandLineSwitches() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Always disable the unsandbox GPU process for DX12 and Vulkan Info
  // collection to avoid interference. This GPU process is launched 120
  // seconds after chrome starts.
  command_line->AppendSwitch(
      switches::kDisableGpuProcessForDX12VulkanInfoCollection);
}

int LaunchTests(TestLauncherDelegate* launcher_delegate,
                size_t parallel_jobs,
                int argc,
                char** argv) {
  DCHECK(!g_launcher_delegate);
  g_launcher_delegate = launcher_delegate;

  base::CommandLine::Init(argc, argv);
  AppendCommandLineSwitches();
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // TODO(tluk) Remove deprecation warning after a few releases. Deprecation
  // warning issued version 79.
  if (command_line->HasSwitch("single_process")) {
    fprintf(stderr, "use --single-process-tests instead of --single_process");
    exit(1);
  }

  if (command_line->HasSwitch(switches::kHelpFlag)) {
    PrintUsage();
    return 0;
  }

#if !defined(OS_ANDROID)
  // The ContentMainDelegate is set for browser tests on Android by the
  // browser test target and is not created by the |launcher_delegate|.
  std::unique_ptr<ContentMainDelegate> content_main_delegate(
      launcher_delegate->CreateContentMainDelegate());
  // ContentMain is not run on Android in the test process, and is run via
  // java for child processes.
  ContentMainParams params(content_main_delegate.get());
#endif

#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  InitializeSandboxInfo(&sandbox_info);

  params.instance = GetModuleHandle(NULL);
  params.sandbox_info = &sandbox_info;
#elif defined(OS_MACOSX)
  sandbox::SeatbeltExecServer::CreateFromArgumentsResult seatbelt =
      sandbox::SeatbeltExecServer::CreateFromArguments(
          command_line->GetProgram().value().c_str(), argc, argv);
  if (seatbelt.sandbox_required) {
    CHECK(seatbelt.server->InitializeSandbox());
  }
#elif !defined(OS_ANDROID)
  params.argc = argc;
  params.argv = const_cast<const char**>(argv);
#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID)
  // This needs to be before trying to run tests as otherwise utility processes
  // end up being launched as a test, which leads to rerunning the test.
  if (command_line->HasSwitch(switches::kProcessType) ||
      command_line->HasSwitch(switches::kLaunchAsBrowser)) {
    return ContentMain(params);
  }
#endif

  if (command_line->HasSwitch(switches::kSingleProcessTests) ||
      (command_line->HasSwitch(switches::kSingleProcess) &&
       command_line->HasSwitch(base::kGTestFilterFlag)) ||
      command_line->HasSwitch(base::kGTestListTestsFlag) ||
      command_line->HasSwitch(base::kGTestHelpFlag)) {
#if !defined(OS_ANDROID)
    g_params = &params;
#endif
    return launcher_delegate->RunTestSuite(argc, argv);
  }

  base::AtExitManager at_exit;
  testing::InitGoogleTest(&argc, argv);
  TestTimeouts::Initialize();

  fprintf(stdout,
          "IMPORTANT DEBUGGING NOTE: each test is run inside its own process.\n"
          "For debugging a test inside a debugger, use the\n"
          "--gtest_filter=<your_test_name> flag along with either\n"
          "--single-process-tests (to run the test in one launcher/browser "
          "process) or\n"
          "--single-process (to do the above, and also run Chrome in single-"
          "process mode).\n");

  base::debug::VerifyDebugger();

  base::SingleThreadTaskExecutor executor(base::MessagePumpType::IO);
#if defined(OS_POSIX)
  base::FileDescriptorWatcher file_descriptor_watcher(executor.task_runner());
#endif

  launcher_delegate->PreSharding();

  WrapperTestLauncherDelegate delegate(launcher_delegate);
  base::TestLauncher launcher(&delegate, parallel_jobs);
  const int result = launcher.Run() ? 0 : 1;
  launcher_delegate->OnDoneRunningTests();
  return result;
}

TestLauncherDelegate* GetCurrentTestLauncherDelegate() {
  return g_launcher_delegate;
}

#if !defined(OS_ANDROID)
ContentMainParams* GetContentMainParams() {
  return g_params;
}
#endif

bool IsPreTest() {
  auto* test = testing::UnitTest::GetInstance();
  return IsPreTestName(test->current_test_info()->name());
}

}  // namespace content
