// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test.h"

#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/launcher/test_launcher_test_utils.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "sandbox/policy/switches.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif

namespace content {

// Disabled on official builds because symbolization in sandboxes processes
// opens up security holes.
// On Android symbolization happens in one step after all the tests ran, so this
// test doesn't work there.
// TODO(mac): figure out why symbolization doesn't happen in the renderer.
// http://crbug.com/521456
// TODO(win): send PDB files for component build. http://crbug.com/521459
#if !defined(OFFICIAL_BUILD) && !BUILDFLAG(IS_ANDROID) && \
    !BUILDFLAG(IS_MAC) && !(defined(COMPONENT_BUILD) && BUILDFLAG(IS_WIN))

namespace {

base::CommandLine CreateCommandLine() {
  const base::CommandLine& cmdline = *base::CommandLine::ForCurrentProcess();
  base::CommandLine command_line = base::CommandLine(cmdline.GetProgram());
#if BUILDFLAG(IS_OZONE)
  static const char* const kSwitchesToCopy[] = {
      // Keep the kOzonePlatform switch that the Ozone must use.
      switches::kOzonePlatform,
  };
  command_line.CopySwitchesFrom(cmdline, kSwitchesToCopy);
#endif
  return command_line;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ContentBrowserTest, MANUAL_ShouldntRun) {
  // Ensures that tests with MANUAL_ prefix don't run automatically.
  ASSERT_TRUE(false);
}

IN_PROC_BROWSER_TEST_F(ContentBrowserTest, MANUAL_RendererCrash) {
  content::RenderProcessHostWatcher renderer_shutdown_observer(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  EXPECT_FALSE(NavigateToURL(shell(), GetWebUIURL("crash")));
  renderer_shutdown_observer.Wait();

  EXPECT_FALSE(renderer_shutdown_observer.did_exit_normally());
}

// Non-Windows sanitizer builds do not symbolize stack traces internally, so use
// this macro to avoid looking for symbols from the stack trace.
#if !BUILDFLAG(IS_WIN) &&                                     \
    (defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) || \
     defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER))
#define USE_EXTERNAL_SYMBOLIZER 1
#else
#define USE_EXTERNAL_SYMBOLIZER 0
#endif

// Tests that browser tests print the callstack when a child process crashes.
// TODO(crbug.com/40834746): Enable this test on Fuchsia once the test
// expectations have been updated.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_RendererCrashCallStack DISABLED_RendererCrashCallStack
#else
#define MAYBE_RendererCrashCallStack RendererCrashCallStack
#endif
IN_PROC_BROWSER_TEST_F(ContentBrowserTest, MAYBE_RendererCrashCallStack) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::CommandLine new_test = CreateCommandLine();
  new_test.AppendSwitchASCII(base::kGTestFilterFlag,
                             "ContentBrowserTest.MANUAL_RendererCrash");
  new_test.AppendSwitch(switches::kRunManualTestsFlag);
  new_test.AppendSwitch(switches::kSingleProcessTests);
  // Test needs to capture stderr so force logging to go there.
  new_test.AppendSwitchASCII(switches::kEnableLogging, "stderr");

#if defined(THREAD_SANITIZER)
  // TSan appears to not be able to report intentional crashes from sandboxed
  // renderer processes.
  new_test.AppendSwitch(sandbox::policy::switches::kNoSandbox);
#endif

  std::string output;
  base::GetAppOutputAndError(new_test, &output);

  // In sanitizer builds, an external script is responsible for symbolizing,
  // so the stack that the tests sees here looks like:
  // "#0 0x0000007ea911 (...content_browsertests+0x7ea910)"
  std::string crash_string =
#if !USE_EXTERNAL_SYMBOLIZER
      "blink::LocalFrameMojoHandler::HandleRendererDebugURL";
#else
      "#0 ";
#endif

  if (!base::Contains(output, crash_string)) {
    GTEST_FAIL() << "Couldn't find\n" << crash_string << "\n in output\n "
                 << output;
  }
}

#ifdef __clang__
// Don't optimize this out of stack traces in ThinLTO builds.
#pragma clang optimize off
#endif
IN_PROC_BROWSER_TEST_F(ContentBrowserTest, MANUAL_BrowserCrash) {
  CHECK(false);
}
#ifdef __clang__
#pragma clang optimize on
#endif

// Tests that browser tests print the callstack on asserts.
// Disabled on Windows crbug.com/1034784
// TODO(crbug.com/40834746): Enable this test on Fuchsia once the test
// expectations have been updated.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)
#define MAYBE_BrowserCrashCallStack DISABLED_BrowserCrashCallStack
#else
#define MAYBE_BrowserCrashCallStack BrowserCrashCallStack
#endif
IN_PROC_BROWSER_TEST_F(ContentBrowserTest, MAYBE_BrowserCrashCallStack) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::CommandLine new_test = CreateCommandLine();
  new_test.AppendSwitchASCII(base::kGTestFilterFlag,
                             "ContentBrowserTest.MANUAL_BrowserCrash");
  new_test.AppendSwitch(switches::kRunManualTestsFlag);
  new_test.AppendSwitch(switches::kSingleProcessTests);
  // A browser process immediate crash can race the initialization of the
  // network service process and leave the process hanging, so run the network
  // service in-process.
  ForceInProcessNetworkService();

  std::string output;
  base::GetAppOutputAndError(new_test, &output);

  // In sanitizer builds, an external script is responsible for symbolizing,
  // so the stack that the test sees here looks like:
  // "#0 0x0000007ea911 (...content_browsertests+0x7ea910)"
  std::string crash_string =
#if !USE_EXTERNAL_SYMBOLIZER
      "content::ContentBrowserTest_MANUAL_BrowserCrash_Test::"
      "RunTestOnMainThread";
#else
      "#0 ";
#endif

  if (!base::Contains(output, crash_string)) {
    GTEST_FAIL() << "Couldn't find\n"
                 << crash_string << "\n in output\n " << output;
  }
}

// The following 3 tests are disabled as they are meant to only run from
// |RunMockTests| to validate tests launcher output for known results.
using MockContentBrowserTest = ContentBrowserTest;

// Basic Test to pass
IN_PROC_BROWSER_TEST_F(MockContentBrowserTest, DISABLED_PassTest) {
  ASSERT_TRUE(true);
}
// Basic Test to fail
IN_PROC_BROWSER_TEST_F(MockContentBrowserTest, DISABLED_FailTest) {
  ASSERT_TRUE(false);
}
// Basic Test to crash
IN_PROC_BROWSER_TEST_F(MockContentBrowserTest, DISABLED_CrashTest) {
  base::ImmediateCrash();
}

// This is disabled due to flakiness: https://crbug.com/1086372
#if BUILDFLAG(IS_WIN)
#define MAYBE_RunMockTests DISABLED_RunMockTests
#elif BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
// This is disabled because it fails on bionic: https://crbug.com/1202220
#define MAYBE_RunMockTests DISABLED_RunMockTests
#else
#define MAYBE_RunMockTests RunMockTests
#endif
// Using TestLauncher to launch 3 simple browser tests
// and validate the resulting json file.
IN_PROC_BROWSER_TEST_F(ContentBrowserTest, MAYBE_RunMockTests) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;

  base::CommandLine command_line = CreateCommandLine();
  command_line.AppendSwitchASCII("gtest_filter",
                                 "MockContentBrowserTest.DISABLED_*");
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path =
      temp_dir.GetPath().AppendASCII("SaveSummaryResult.json");
  command_line.AppendSwitchPath("test-launcher-summary-output", path);
  command_line.AppendSwitch("gtest_also_run_disabled_tests");
  command_line.AppendSwitchASCII("test-launcher-retry-limit", "0");

  std::string output;
  base::GetAppOutputAndError(command_line, &output);

  // Validate the resulting JSON file is the expected output.
  std::optional<base::Value::Dict> root =
      base::test_launcher_utils::ReadSummary(path);
  ASSERT_TRUE(root);

  base::Value::Dict* dict = root->FindDict("test_locations");
  ASSERT_TRUE(dict);
  EXPECT_EQ(3u, dict->size());
  EXPECT_TRUE(base::test_launcher_utils::ValidateTestLocations(
      *dict, "MockContentBrowserTest"));

  base::Value::List* list = root->FindList("per_iteration_data");
  ASSERT_TRUE(list);
  ASSERT_EQ(1u, list->size());

  base::Value::Dict* iteration_dict = (*list)[0].GetIfDict();
  ASSERT_TRUE(iteration_dict);
  EXPECT_EQ(3u, iteration_dict->size());
  // We expect the result to be stripped of disabled prefix.
  EXPECT_TRUE(base::test_launcher_utils::ValidateTestResult(
      *iteration_dict, "MockContentBrowserTest.PassTest", "SUCCESS", 0u));
  EXPECT_TRUE(base::test_launcher_utils::ValidateTestResult(
      *iteration_dict, "MockContentBrowserTest.FailTest", "FAILURE", 1u));
  EXPECT_TRUE(base::test_launcher_utils::ValidateTestResult(
      *iteration_dict, "MockContentBrowserTest.CrashTest", "CRASH", 0u));
}

#endif

class ContentBrowserTestSanityTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_FALSE(ran_);

    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (std::string(test_info->name()) == "SingleProcess")
      command_line->AppendSwitch(switches::kSingleProcess);
  }

  void SetUp() override {
    ASSERT_FALSE(ran_);
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override { ASSERT_FALSE(ran_); }

  void Test() {
    ASSERT_FALSE(ran_);
    ran_ = true;

    GURL url = GetTestUrl(".", "simple_page.html");

    std::u16string expected_title(u"OK");
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    std::u16string title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, title);
  }

  void TearDownOnMainThread() override { ASSERT_TRUE(ran_); }

  void TearDown() override {
    ASSERT_TRUE(ran_);
    ContentBrowserTest::TearDown();
  }

 private:
  // Verify that Test() is invoked once and only once between SetUp and TearDown
  // phases.
  bool ran_ = false;
};

IN_PROC_BROWSER_TEST_F(ContentBrowserTestSanityTest, Basic) {
  Test();
}

IN_PROC_BROWSER_TEST_F(ContentBrowserTestSanityTest, SingleProcess) {
  Test();
}

namespace {

BASE_FEATURE(kTestFeatureForBrowserTest1,
             "TestFeatureForBrowserTest1",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeatureForBrowserTest2,
             "TestFeatureForBrowserTest2",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeatureForBrowserTest3,
             "TestFeatureForBrowserTest3",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeatureForBrowserTest4,
             "TestFeatureForBrowserTest4",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

class ContentBrowserTestScopedFeatureListTest : public ContentBrowserTest {
 public:
  ContentBrowserTestScopedFeatureListTest() {
    scoped_feature_list_.InitWithFeatures({kTestFeatureForBrowserTest3},
                                          {kTestFeatureForBrowserTest4});
  }

  ContentBrowserTestScopedFeatureListTest(
      const ContentBrowserTestScopedFeatureListTest&) = delete;
  ContentBrowserTestScopedFeatureListTest& operator=(
      const ContentBrowserTestScopedFeatureListTest&) = delete;

  ~ContentBrowserTestScopedFeatureListTest() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContentBrowserTestScopedFeatureListTest,
                       FeatureListTest) {
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTestFeatureForBrowserTest1));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTestFeatureForBrowserTest2));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTestFeatureForBrowserTest3));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTestFeatureForBrowserTest4));
}

namespace {

void CallbackChecker(bool* non_nested_task_ran) {
  *non_nested_task_ran = true;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ContentBrowserTest, NonNestableTask) {
  bool non_nested_task_ran = false;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostNonNestableTask(
      FROM_HERE, base::BindOnce(&CallbackChecker, &non_nested_task_ran));
  content::RunAllPendingInMessageLoop();
  ASSERT_TRUE(non_nested_task_ran);
}

IN_PROC_BROWSER_TEST_F(ContentBrowserTest, RunTimeoutInstalled) {
  // Verify that a RunLoop timeout is installed and shorter than the test
  // timeout itself.
  const base::RunLoop::RunLoopTimeout* run_timeout =
      base::test::ScopedRunLoopTimeout::GetTimeoutForCurrentThread();
  EXPECT_TRUE(run_timeout);
  EXPECT_LT(run_timeout->timeout, TestTimeouts::test_launcher_timeout());

  static auto& static_on_timeout_cb = run_timeout->on_timeout;
#if defined(__clang__) && defined(_MSC_VER)
  EXPECT_NONFATAL_FAILURE(
      static_on_timeout_cb.Run(FROM_HERE),
      "RunLoop::Run() timed out. Timeout set at "
      // We don't test the line number but it would be present.
      "ProxyRunTestOnMainThreadLoop@content\\public\\test\\"
      "browser_test_base.cc:");
#else
  EXPECT_NONFATAL_FAILURE(
      static_on_timeout_cb.Run(FROM_HERE),
      "RunLoop::Run() timed out. Timeout set at "
      // We don't test the line number but it would be present.
      "ProxyRunTestOnMainThreadLoop@content/public/test/"
      "browser_test_base.cc:");
#endif
}

enum class SkipLocation {
  kSetUpInProcessBrowserTestFixture,
  kSetUpCommandLine,
  kSetUpOnMainThread,
};

class SkipFixtureBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<SkipLocation> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    if (GetParam() == SkipLocation::kSetUpInProcessBrowserTestFixture) {
      GTEST_SKIP();
    }
  }

  void SetUpCommandLine(base::CommandLine*) override {
    if (GetParam() == SkipLocation::kSetUpCommandLine) {
      GTEST_SKIP();
    }
  }

  void SetUpOnMainThread() override {
    if (GetParam() == SkipLocation::kSetUpOnMainThread) {
      GTEST_SKIP();
    }
  }
};

IN_PROC_BROWSER_TEST_P(SkipFixtureBrowserTest, Skip) {
  EXPECT_TRUE(false);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SkipFixtureBrowserTest,
    ::testing::Values(SkipLocation::kSetUpInProcessBrowserTestFixture,
                      SkipLocation::kSetUpCommandLine,
                      SkipLocation::kSetUpOnMainThread));

// This test verifies that CreateUniqueTempDir always creates a dir underneath
// the temp directory when running in a browser test. This is needed because, on
// Windows, when running elevated, CreateUniqueTempDir would normally create a
// temp dir in a secure location (e.g. %systemroot%\SystemTemp) but, for browser
// tests, this behavior is explicitly overridden to avoid leaving temp files in
// this system dir after tests complete. See BrowserTestBase::BrowserTestBase.
IN_PROC_BROWSER_TEST_F(ContentBrowserTest, TempPathLocation) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::ScopedTempDir scoped_dir;
  EXPECT_TRUE(scoped_dir.CreateUniqueTempDir());

  base::FilePath temp_path = base::PathService::CheckedGet(base::DIR_TEMP);
  EXPECT_TRUE(temp_path.IsParent(scoped_dir.GetPath()));
}

}  // namespace content
