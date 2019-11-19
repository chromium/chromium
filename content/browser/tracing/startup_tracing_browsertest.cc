// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/tracing/common/trace_startup_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/browser/tracing/perfetto_file_tracer.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "services/tracing/perfetto/privacy_filtering_check.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "services/tracing/public/cpp/tracing_features.h"

namespace content {

namespace {

// Wait until |condition| returns true.
void WaitForCondition(base::RepeatingCallback<bool()> condition,
                      const std::string& description) {
  const base::TimeDelta kTimeout = base::TimeDelta::FromSeconds(15);
  const base::TimeTicks start_time = base::TimeTicks::Now();
  while (!condition.Run() && (base::TimeTicks::Now() - start_time < kTimeout)) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
  ASSERT_TRUE(condition.Run())
      << "Timeout waiting for condition: " << description;
}

}  // namespace

class CommandlineStartupTracingTest : public ContentBrowserTest {
 public:
  CommandlineStartupTracingTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CreateTemporaryFile(&temp_file_path_);
    command_line->AppendSwitch(switches::kTraceStartup);
    command_line->AppendSwitchASCII(switches::kTraceStartupDuration, "3");
    command_line->AppendSwitchASCII(switches::kTraceStartupFile,
                                    temp_file_path_.AsUTF8Unsafe());

#if defined(OS_ANDROID)
    // On Android the startup tracing is initialized as soon as library load
    // time, earlier than this point. So, reset the config and enable startup
    // tracing here.
    tracing::TraceStartupConfig::GetInstance()->EnableFromCommandLine();
    tracing::EnableStartupTracingIfNeeded();
#endif
  }

 protected:
  base::FilePath temp_file_path_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CommandlineStartupTracingTest);
};

IN_PROC_BROWSER_TEST_F(CommandlineStartupTracingTest, TestStartupTracing) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "title1.html")));
  WaitForCondition(base::BindRepeating([]() {
                     return !TracingController::GetInstance()->IsTracing();
                   }),
                   "trace end");
  EXPECT_FALSE(tracing::TraceStartupConfig::GetInstance()->IsEnabled());
  EXPECT_FALSE(TracingController::GetInstance()->IsTracing());
  WaitForCondition(base::BindRepeating([]() {
                     return tracing::TraceStartupConfig::GetInstance()
                         ->finished_writing_to_file_for_testing();
                   }),
                   "finish file write");

  std::string trace;
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::ReadFileToString(temp_file_path_, &trace));
  EXPECT_TRUE(
      trace.find("TracingControllerImpl::InitStartupTracingForDuration") !=
      std::string::npos);
}

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
  ~LargeTraceEventData() override = default;

  const size_t kLargeMessageSize = 100 * 1024;
  void AppendAsTraceFormat(std::string* out) const override {
    std::string large_string(kLargeMessageSize, '.');
    out->append(large_string);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LargeTraceEventData);
};

// This will fill a massive amount of startup tracing data into a
// StartupTraceWriter, which Perfetto will then have to sync copy into
// the SMB once the full tracing service starts up. This is to catch common
// deadlocks.
IN_PROC_BROWSER_TEST_F(StartupTracingInProcessTest, TestFilledStartupBuffer) {
  tracing::TraceEventDataSource::GetInstance()->SetupStartupTracing(
      /*privacy_filtering_enabled=*/false);

  auto config = tracing::TraceStartupConfig::GetInstance()
                    ->GetDefaultBrowserStartupConfig();
  config.SetTraceBufferSizeInEvents(0);
  config.SetTraceBufferSizeInKb(0);
  uint8_t modes = base::trace_event::TraceLog::RECORDING_MODE;
  base::trace_event::TraceLog::GetInstance()->SetEnabled(config, modes);

  for (int i = 0; i < 1024; ++i) {
    auto data = std::make_unique<LargeTraceEventData>();
    TRACE_EVENT1("toplevel", "bar", "data", std::move(data));
  }

  config.SetTraceBufferSizeInKb(32);

  base::RunLoop wait_for_tracing;
  TracingControllerImpl::GetInstance()->StartTracing(
      config, wait_for_tracing.QuitClosure());
  wait_for_tracing.Run();

  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "title1.html")));

  base::RunLoop wait_for_stop;
  TracingControllerImpl::GetInstance()->StopTracing(
      TracingController::CreateStringEndpoint(base::BindOnce(
          [](base::OnceClosure quit_callback,
             std::unique_ptr<std::string> data) {
            std::move(quit_callback).Run();
          },
          wait_for_stop.QuitClosure())));
  wait_for_stop.Run();
}

class BackgroundStartupTracingTest : public ContentBrowserTest {
 public:
  BackgroundStartupTracingTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CreateTemporaryFile(&temp_file_path_);
    auto* startup_config = tracing::TraceStartupConfig::GetInstance();
    startup_config->enable_background_tracing_for_testing_ = true;
    startup_config->EnableFromBackgroundTracing();
    startup_config->startup_duration_in_seconds_ = 3;
    tracing::EnableStartupTracingIfNeeded();
    command_line->AppendSwitchASCII(switches::kPerfettoOutputFile,
                                    temp_file_path_.AsUTF8Unsafe());
  }

 protected:
  base::FilePath temp_file_path_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundStartupTracingTest);
};

#if !defined(OS_ANDROID)
#define MAYBE_TestStartupTracing DISABLED_TestStartupTracing
#else
#define MAYBE_TestStartupTracing TestStartupTracing
#endif
IN_PROC_BROWSER_TEST_F(BackgroundStartupTracingTest, MAYBE_TestStartupTracing) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "title1.html")));

  EXPECT_FALSE(tracing::TraceStartupConfig::GetInstance()->IsEnabled());
  EXPECT_FALSE(TracingController::GetInstance()->IsTracing());
  WaitForCondition(base::BindRepeating([]() {
                     return TracingControllerImpl::GetInstance()
                         ->perfetto_file_tracer_for_testing()
                         ->is_finished_for_testing();
                   }),
                   "finish file write");

  std::string trace;
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::ReadFileToString(temp_file_path_, &trace));
  tracing::PrivacyFilteringCheck checker;
  checker.CheckProtoForUnexpectedFields(trace);
  EXPECT_GT(checker.stats().track_event, 0u);
  EXPECT_GT(checker.stats().process_desc, 0u);
  EXPECT_GT(checker.stats().thread_desc, 0u);
  EXPECT_TRUE(checker.stats().has_interned_names);
  EXPECT_TRUE(checker.stats().has_interned_categories);
  EXPECT_TRUE(checker.stats().has_interned_source_locations);
}

}  // namespace content
