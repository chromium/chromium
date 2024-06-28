// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/dcheck_is_on.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/browser/tracing/startup_tracing_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "services/tracing/perfetto/privacy_filtering_check.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace content {

namespace {

void CheckForConditionAndWaitMoreIfNeeded(
    base::RepeatingCallback<bool()> condition,
    base::OnceClosure quit_closure) {
  if (condition.Run()) {
    std::move(quit_closure).Run();
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CheckForConditionAndWaitMoreIfNeeded,
                     std::move(condition), std::move(quit_closure)),
      TestTimeouts::tiny_timeout());
}

// Wait until |condition| returns true.
void WaitForCondition(base::RepeatingCallback<bool()> condition,
                      const std::string& description) {
  base::RunLoop run_loop;
  CheckForConditionAndWaitMoreIfNeeded(condition, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(condition.Run())
      << "Timeout waiting for condition: " << description;
}

}  // namespace

class StartupTracingInProcessTest : public ContentBrowserTest {
 public:
  StartupTracingInProcessTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kTracingServiceInProcess},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class LargeTraceEventData : public base::trace_event::ConvertableToTraceFormat {
 public:
  LargeTraceEventData() = default;

  LargeTraceEventData(const LargeTraceEventData&) = delete;
  LargeTraceEventData& operator=(const LargeTraceEventData&) = delete;

  ~LargeTraceEventData() override = default;

  const size_t kLargeMessageSize = 100 * 1024;
  void AppendAsTraceFormat(std::string* out) const override {
    std::string large_string(kLargeMessageSize, '.');
    out->append(large_string);
  }
};

// This will fill a massive amount of startup tracing data into a
// StartupTraceWriter, which Perfetto will then have to sync copy into
// the SMB once the full tracing service starts up. This is to catch common
// deadlocks.
// TODO(crbug.com/330909115): Re-enable this test.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_TestFilledStartupBuffer DISABLED_TestFilledStartupBuffer
#else
#define MAYBE_TestFilledStartupBuffer TestFilledStartupBuffer
#endif
IN_PROC_BROWSER_TEST_F(StartupTracingInProcessTest,
                       MAYBE_TestFilledStartupBuffer) {
  auto config = tracing::TraceStartupConfig::GetInstance()
                    .GetDefaultBackgroundStartupConfig();

  CHECK(tracing::EnableStartupTracingForProcess(config));

  for (int i = 0; i < 1024; ++i) {
    auto data = std::make_unique<LargeTraceEventData>();
    TRACE_EVENT1("toplevel", "bar", "data", std::move(data));
  }

  base::RunLoop wait_for_tracing;
  auto session =
      perfetto::Tracing::NewTrace(perfetto::BackendType::kCustomBackend);
  session->Setup(config);
  session->SetOnStartCallback(
      [&wait_for_tracing]() { wait_for_tracing.Quit(); });
  session->Start();
  wait_for_tracing.Run();

  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "title1.html")));

  base::RunLoop wait_for_stop;
  session->SetOnStopCallback([&wait_for_stop]() { wait_for_stop.Quit(); });
  session->Stop();
  wait_for_stop.Run();
}

namespace {

enum class FinishType {
  kWaitForTimeout,
  kStopExplicitly,
};

std::ostream& operator<<(std::ostream& o, FinishType type) {
  switch (type) {
    case FinishType::kStopExplicitly:
      o << "Stop";
      return o;
    case FinishType::kWaitForTimeout:
      o << "Wait";
      return o;
  }
}

enum class OutputType {
  kProto,
  kJSON,
};

std::ostream& operator<<(std::ostream& o, OutputType type) {
  switch (type) {
    case OutputType::kJSON:
      o << "json";
      return o;
    case OutputType::kProto:
      o << "proto";
      return o;
  }
}

enum class OutputLocation {
  // Write trace to a given file.
  kGivenFile,
  // Write trace into a given directory (basename will be set to trace1 before
  // starting).
  kDirectoryWithDefaultBasename,
  // Write trace into a given directory (basename will be set to trace1 before
  // starting, and updated to trace2 before calling Stop()).
  kDirectoryWithBasenameUpdatedBeforeStop,
};

std::ostream& operator<<(std::ostream& o, OutputLocation type) {
  switch (type) {
    case OutputLocation::kGivenFile:
      o << "file";
      return o;
    case OutputLocation::kDirectoryWithDefaultBasename:
      o << "dir/trace1";
      return o;
    case OutputLocation::kDirectoryWithBasenameUpdatedBeforeStop:
      o << "dir/trace2";
      return o;
  }
}

}  // namespace

class StartupTracingTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<
          std::tuple<FinishType, OutputType, OutputLocation>> {
 public:
  StartupTracingTest() = default;

  StartupTracingTest(const StartupTracingTest&) = delete;
  StartupTracingTest& operator=(const StartupTracingTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kTraceStartup);
    if (GetFinishType() == FinishType::kWaitForTimeout) {
      command_line->AppendSwitchASCII(switches::kTraceStartupDuration, "3");
    } else {
      command_line->AppendSwitchASCII(switches::kTraceStartupDuration, "0");
    }
    command_line->AppendSwitchASCII(switches::kTraceStartupFormat,
                                    GetOutputTypeAsString());

    if (GetOutputLocation() == OutputLocation::kGivenFile) {
      base::CreateTemporaryFile(&temp_file_path_);
    } else {
      base::CreateNewTempDirectory(base::FilePath::StringType(),
                                   &temp_file_path_);
      temp_file_path_ = temp_file_path_.AsEndingWithSeparator();
    }

    command_line->AppendSwitchASCII(switches::kEnableTracingOutput,
                                    temp_file_path_.AsUTF8Unsafe());

    if (GetOutputLocation() != OutputLocation::kGivenFile) {
      // --enable-tracing-format switch should be initialised before
      // calling SetDefaultBasenameForTest, which forces the creation of
      // TraceStartupConfig, which queries the command line flags and
      // stores the snapshot.
      StartupTracingController::GetInstance().SetDefaultBasenameForTest(
          "trace1",
          StartupTracingController::ExtensionType::kAppendAppropriate);
    }
  }

  FinishType GetFinishType() { return std::get<0>(GetParam()); }

  OutputType GetOutputType() { return std::get<1>(GetParam()); }

  std::string GetOutputTypeAsString() {
    switch (GetOutputType()) {
      case OutputType::kJSON:
        return "json";
      case OutputType::kProto:
        return "pftrace";
    }
  }

  OutputLocation GetOutputLocation() { return std::get<2>(GetParam()); }

  base::FilePath GetExpectedPath() {
    std::string filename;

    switch (GetOutputLocation()) {
      case OutputLocation::kGivenFile:
        return temp_file_path_;
      case OutputLocation::kDirectoryWithDefaultBasename:
        filename = "trace1";
        break;
      case OutputLocation::kDirectoryWithBasenameUpdatedBeforeStop:
        filename = "trace2";
        break;
    }

    // Renames are not supported together with timeouts.
    if (GetFinishType() == FinishType::kWaitForTimeout)
      filename = "trace1";

    return temp_file_path_.AppendASCII(filename + "." +
                                       GetOutputTypeAsString());
  }

  static void CheckOutput(base::FilePath path, OutputType output_type) {
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
    // Skip checks because the thread sanitizer is often too slow to flush trace
    // data correctly within the timeouts. We still run the tests on TSAN to
    // catch general threading issues.
#else   // !(BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER))
    std::string trace;
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(path, &trace))
        << "Failed to read file " << path;

    if (output_type == OutputType::kJSON) {
      EXPECT_TRUE(base::JSONReader::Read(trace));
    }

    // Both proto and json should have the trace event name recorded somewhere
    // as a substring. We check for "ThreadControllerImpl::RunTask" because
    // it's an example of event that happens early in the trace, but any other
    // early event will do. The event has to happen early because in
    // WaitForTimeout and in EmergencyStop tests we don't wait for
    // TracingSession::StartBlocking() to complete.
    EXPECT_TRUE(trace.find("ThreadControllerImpl::RunTask") !=
                std::string::npos);
#endif  // !(BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER))
  }

  void Wait() {
    if (GetFinishType() == FinishType::kWaitForTimeout) {
      WaitForCondition(base::BindRepeating([]() {
                         return StartupTracingController::GetInstance()
                             .is_finished_for_testing();
                       }),
                       "finish file write");
    } else {
      StartupTracingController::GetInstance().WaitUntilStopped();
    }
  }

 protected:
  base::FilePath temp_file_path_;

 private:
  base::test::ScopedRunLoopTimeout increased_timeout_{
      FROM_HERE, TestTimeouts::test_launcher_timeout()};
};

INSTANTIATE_TEST_SUITE_P(
    All,
    StartupTracingTest,
    testing::Combine(
        testing::Values(FinishType::kStopExplicitly,
                        FinishType::kWaitForTimeout),
        testing::Values(OutputType::kJSON, OutputType::kProto),
        testing::Values(
            OutputLocation::kGivenFile,
            OutputLocation::kDirectoryWithDefaultBasename,
            OutputLocation::kDirectoryWithBasenameUpdatedBeforeStop)));

// TODO(crbug.com/40900782): Re-enable this test.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_TestEnableTracing DISABLED_TestEnableTracing
#else
#define MAYBE_TestEnableTracing TestEnableTracing
#endif
IN_PROC_BROWSER_TEST_P(StartupTracingTest, MAYBE_TestEnableTracing) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "title1.html")));

  if (GetOutputLocation() ==
      OutputLocation::kDirectoryWithBasenameUpdatedBeforeStop) {
    StartupTracingController::GetInstance().SetDefaultBasenameForTest(
        "trace2", StartupTracingController::ExtensionType::kAppendAppropriate);
  }

  Wait();

  CheckOutput(GetExpectedPath(), GetOutputType());
}

// TODO(ssid): Fix the flaky tests, probably the same reason as
// crbug.com/1041392.
IN_PROC_BROWSER_TEST_P(StartupTracingTest, DISABLED_ContinueAtShutdown) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "title1.html")));
  StartupTracingController::GetInstance()
      .set_continue_on_shutdown_for_testing();
}

class EmergencyStopTracingTest : public StartupTracingTest {};

INSTANTIATE_TEST_SUITE_P(
    All,
    EmergencyStopTracingTest,
    testing::Combine(
        testing::Values(FinishType::kStopExplicitly),
        testing::Values(OutputType::kJSON, OutputType::kProto),
        testing::Values(OutputLocation::kDirectoryWithDefaultBasename)));

// TODO(crbug.com/40900782): Re-enable this test.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_StopOnUIThread DISABLED_StopOnUIThread
#else
#define MAYBE_StopOnUIThread StopOnUIThread
#endif
IN_PROC_BROWSER_TEST_P(EmergencyStopTracingTest, MAYBE_StopOnUIThread) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "title1.html")));

  StartupTracingController::EmergencyStop();
  CheckOutput(GetExpectedPath(), GetOutputType());
}

// TODO(crbug.com/40900782): Re-enable this test.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_StopOnThreadPool DISABLED_StopOnThreadPool
#else
#define MAYBE_StopOnThreadPool StopOnThreadPool
#endif
IN_PROC_BROWSER_TEST_P(EmergencyStopTracingTest, MAYBE_StopOnThreadPool) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "title1.html")));

  auto expected_path = GetExpectedPath();
  auto output_type = GetOutputType();

  base::RunLoop run_loop;

  base::ThreadPool::PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                               StartupTracingController::EmergencyStop();
                               CheckOutput(expected_path, output_type);
                               run_loop.Quit();
                             }));

  run_loop.Run();
}

// TODO(crbug.com/40900782): Re-enable this test.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_StopOnThreadPoolTwice DISABLED_StopOnThreadPoolTwice
#else
#define MAYBE_StopOnThreadPoolTwice StopOnThreadPoolTwice
#endif
IN_PROC_BROWSER_TEST_P(EmergencyStopTracingTest, MAYBE_StopOnThreadPoolTwice) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "title1.html")));

  auto expected_path = GetExpectedPath();
  auto output_type = GetOutputType();

  base::RunLoop run_loop1;
  base::RunLoop run_loop2;

  base::ThreadPool::PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                               StartupTracingController::EmergencyStop();
                               CheckOutput(expected_path, output_type);
                               run_loop1.Quit();
                             }));
  base::ThreadPool::PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                               StartupTracingController::EmergencyStop();
                               CheckOutput(expected_path, output_type);
                               run_loop2.Quit();
                             }));

  run_loop1.Run();
  run_loop2.Run();
}

}  // namespace content
