// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tracing/ios_tracing_controller.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/run_loop.h"
#import "base/strings/string_number_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/bind.h"
#import "base/test/run_until.h"
#import "base/test/scoped_command_line.h"
#import "base/test/task_environment.h"
#import "base/test/tracing/test_trace_processor.h"
#import "base/trace_event/trace_event.h"
#import "base/trace_event/trace_log.h"
#import "base/tracing/perfetto_platform.h"
#import "components/tracing/common/tracing_switches.h"
#import "services/tracing/public/cpp/background_tracing/trace_report_database.h"
#import "services/tracing/public/cpp/background_tracing/tracing_scenario.h"
#import "services/tracing/public/cpp/perfetto/perfetto_data_source_names.h"
#import "services/tracing/public/cpp/startup_tracing_controller.h"
#import "services/tracing/public/cpp/trace_startup_config.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/perfetto/protos/perfetto/config/chrome/scenario_config.gen.h"
#import "third_party/perfetto/protos/perfetto/config/data_source_config.gen.h"

namespace {

size_t GetDatabaseReportCount(IOSTracingController& instance) {
  size_t result_count = 0;
  base::RunLoop run_loop;
  instance.GetAllTraceReports(base::BindOnce(
      [](base::RunLoop* run_loop, size_t* out_count,
         std::vector<tracing::ClientTraceReport> reports) {
        *out_count = reports.size();
        run_loop->Quit();
      },
      base::Unretained(&run_loop), base::Unretained(&result_count)));
  run_loop.Run();
  return result_count;
}

}  // namespace

class IOSTracingControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    IOSTracingController::MaybeCreateInstanceForTesting();
    IOSTracingController::GetInstance().InitializeForTesting();
  }

  void TearDown() override {
    IOSTracingController::GetInstance().ResetForTesting();
    PlatformTest::TearDown();
  }

  bool IsRecordingAllowed(IOSTracingController& instance,
                          bool privacy_filter_enabled,
                          base::TimeTicks scenario_start_time) {
    return instance.IsRecordingAllowed(privacy_filter_enabled,
                                       scenario_start_time);
  }

  base::test::TaskEnvironment task_environment_;
};

// Tests that the manager successfully creates the standard developer
// TraceConfig and does not crash or leave invalid defaults.
TEST_F(IOSTracingControllerTest, CreatesValidDeveloperConfig) {
  perfetto::TraceConfig config =
      IOSTracingController::GetInstance().CreateDeveloperTraceConfig();

  // Validate the buffer size is set to the 50MB default.
  ASSERT_GT(config.buffers_size(), 0);
  EXPECT_EQ(config.buffers()[0].size_kb(), 1024u * 50u);

  // Validate that the data sources are enabled.
  bool found_track_event = false;
  bool found_sampler_profiler = false;
  bool found_system_metrics = false;
  bool found_metadata = false;
  bool found_histogram = false;

  for (const auto& ds : config.data_sources()) {
    const std::string& name = ds.config().name();
    if (name == "track_event") {
      found_track_event = true;
    } else if (name == tracing::kSamplerProfilerSourceName) {
      found_sampler_profiler = true;
      EXPECT_FALSE(ds.config().chrome_config().privacy_filtering_enabled());
    } else if (name == tracing::kSystemMetricsSourceName) {
      found_system_metrics = true;
    } else if (name == tracing::kMetaData2SourceName) {
      found_metadata = true;
    } else if (name == tracing::kHistogramSampleSourceName) {
      found_histogram = true;
    }
  }

  EXPECT_TRUE(found_track_event);
  EXPECT_TRUE(found_sampler_profiler);
  EXPECT_TRUE(found_system_metrics);
  EXPECT_TRUE(found_metadata);
  EXPECT_TRUE(found_histogram);
}

// Tests that a startup trace session can be started and recorded.
TEST_F(IOSTracingControllerTest, StartupTraceRecording) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath trace_file =
      temp_dir.GetPath().AppendASCII("startup_trace.proto");

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kTraceStartup);
  scoped_command_line.GetProcessCommandLine()->AppendSwitchPath(
      switches::kTraceStartupFile, trace_file);
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kTraceStartupFormat, "proto");

  // Reset the config to pick up the new command line switches.
  tracing::TraceStartupConfig::ResetForTesting();

  // Reset and re-initialize to restart startup tracing.
  IOSTracingController::GetInstance().ResetForTesting();
  base::ThreadPoolInstance::Get()->FlushForTesting();
  IOSTracingController::GetInstance().InitializeForTesting();

  // Wait for the background tracer to start blocking and actually begin
  // tracing.
  base::ThreadPoolInstance::Get()->FlushForTesting();

  {
    TRACE_EVENT("startup", "StartupTestEvent");
  }

  // Wait for the trace to be flushed and stopped.
  IOSTracingController::GetInstance()
      .startup_tracing_controller()
      ->ShutdownAndWaitForStopIfNeeded();

  // Verify
  std::string trace_data;
  ASSERT_TRUE(base::PathExists(trace_file));
  ASSERT_TRUE(base::ReadFileToString(trace_file, &trace_data));
  ASSERT_FALSE(trace_data.empty());

  base::test::TestTraceProcessorImpl ttp;
  ASSERT_TRUE(
      ttp.ParseTrace(std::vector<char>(trace_data.begin(), trace_data.end()))
          .ok());

  auto result = ttp.ExecuteQuery(
      "SELECT count(*) FROM slice WHERE name = 'StartupTestEvent'");
  ASSERT_TRUE(result.ok()) << result.error();
  int count = 0;
  base::StringToInt(result.result()[1][0], &count);
  EXPECT_GE(count, 1);
}

// Tests that a manual trace session can be started and recorded.
TEST_F(IOSTracingControllerTest, ManualTraceRecording) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath trace_file =
      temp_dir.GetPath().AppendASCII("manual_trace.proto");

  {
    perfetto::TraceConfig config =
        IOSTracingController::GetInstance().CreateDeveloperTraceConfig();

    // Ensure track_event is enabled.
    bool found_track_event = false;
    for (const auto& ds : config.data_sources()) {
      if (ds.config().name() == "track_event") {
        found_track_event = true;
        break;
      }
    }
    if (!found_track_event) {
      config.add_data_sources()->mutable_config()->set_name("track_event");
    }

    auto tracing_session =
        perfetto::Tracing::NewTrace(perfetto::kInProcessBackend);

    base::File file(trace_file,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());

    tracing_session->Setup(config, file.TakePlatformFile());

    base::RunLoop start_run_loop;
    tracing_session->SetOnStartCallback(
        [&start_run_loop]() { start_run_loop.Quit(); });
    tracing_session->Start();
    start_run_loop.Run();

    TRACE_EVENT("base", "ManualTestEvent");

    base::RunLoop flush_run_loop;
    tracing_session->Flush([&flush_run_loop](bool) { flush_run_loop.Quit(); });
    flush_run_loop.Run();

    base::RunLoop stop_run_loop;
    tracing_session->SetOnStopCallback(
        [&stop_run_loop]() { stop_run_loop.Quit(); });
    tracing_session->Stop();
    stop_run_loop.Run();
  }

  // Verify
  std::string trace_data;
  ASSERT_TRUE(base::PathExists(trace_file));
  ASSERT_TRUE(base::ReadFileToString(trace_file, &trace_data));
  ASSERT_FALSE(trace_data.empty());

  base::test::TestTraceProcessorImpl ttp;
  ASSERT_TRUE(
      ttp.ParseTrace(std::vector<char>(trace_data.begin(), trace_data.end()))
          .ok());

  auto result = ttp.ExecuteQuery(
      "SELECT count(*) FROM slice WHERE name = 'ManualTestEvent'");
  ASSERT_TRUE(result.ok()) << result.error();
  int count = 0;
  base::StringToInt(result.result()[1][0], &count);
  EXPECT_GE(count, 1);
}

TEST_F(IOSTracingControllerTest, UploadFlow) {
  base::ThreadPoolInstance::Get()->FlushForTesting();

  perfetto::protos::gen::TriggerRule config;
  config.set_manual_trigger_name("test_trigger");
  auto rule = tracing::BackgroundTracingRule::Create(config);

  perfetto::protos::gen::ScenarioConfig scenario_config;
  scenario_config.set_scenario_name("test_scenario");
  auto scenario = tracing::TracingScenario::Create(
      scenario_config, /*enable_privacy_filter=*/false,
      /*is_local_scenario=*/false, /*enable_package_name_filter=*/false,
      /*request_startup_tracing=*/false,
      static_cast<tracing::TracingScenario::Delegate*>(
          &IOSTracingController::GetInstance()));

  base::Token uuid = base::Token::CreateRandom();

  std::string fake_trace = "fake trace data";
  IOSTracingController::GetInstance().SaveTraceForTesting(
      std::move(fake_trace), scenario->scenario_name(), rule->rule_name(),
      uuid);

  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_TRUE(base::test::RunUntil([]() -> bool {
    return IOSTracingController::GetInstance().HasTraceToUpload();
  }));

  bool called = false;
  IOSTracingController::GetInstance().GetTraceToUpload(
      base::BindLambdaForTesting([&](std::optional<std::string> content,
                                     std::optional<std::string> system_profile,
                                     base::OnceClosure upload_complete) {
        called = true;
        EXPECT_TRUE(content.has_value());
        if (content.has_value()) {
          EXPECT_FALSE(content->empty());
        }
        std::move(upload_complete).Run();
      }));

  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_TRUE(base::test::RunUntil([&] { return called; }));

  EXPECT_TRUE(called);
}

TEST_F(IOSTracingControllerTest, InitializeFieldScenarios) {
  perfetto::protos::gen::ChromeFieldTracingConfig config;
  auto* scenario = config.add_scenarios();
  scenario->set_scenario_name("test_scenario");

  bool result = IOSTracingController::GetInstance().InitializeFieldScenarios(
      config, tracing::BackgroundTracingManager::ANONYMIZE_DATA,
      /*force_uploads=*/false,
      /*upload_limit_kb=*/0);

  EXPECT_TRUE(result);
  EXPECT_FALSE(IOSTracingController::GetInstance().HasTraceToUpload());
}

TEST_F(IOSTracingControllerTest, OversizedLogSuppression) {
  base::ThreadPoolInstance::Get()->FlushForTesting();

  auto& instance = IOSTracingController::GetInstance();

  perfetto::protos::gen::ChromeFieldTracingConfig config;
  auto* scenario_cfg = config.add_scenarios();
  scenario_cfg->set_scenario_name("test_scenario");

  instance.InitializeFieldScenarios(
      config, tracing::BackgroundTracingManager::ANONYMIZE_DATA,
      /*force_uploads=*/false,
      /*upload_limit_kb_=*/1);

  std::string big_trace(2048, 'a');

  perfetto::protos::gen::TriggerRule rule_cfg;
  rule_cfg.set_manual_trigger_name("test_trigger");
  auto rule = tracing::BackgroundTracingRule::Create(rule_cfg);

  perfetto::protos::gen::ScenarioConfig proto_scenario_cfg;
  proto_scenario_cfg.set_scenario_name("test_scenario");
  auto scenario = tracing::TracingScenario::Create(
      proto_scenario_cfg, /*enable_privacy_filter=*/false,
      /*is_local_scenario=*/false, /*enable_package_name_filter=*/false,
      /*request_startup_tracing=*/false,
      static_cast<tracing::TracingScenario::Delegate*>(&instance));

  base::Token uuid = base::Token::CreateRandom();
  instance.SaveTraceForTesting(std::move(big_trace), scenario->scenario_name(),
                               rule->rule_name(), uuid);

  base::ThreadPoolInstance::Get()->FlushForTesting();

  EXPECT_EQ(GetDatabaseReportCount(instance), 1u);
}

TEST_F(IOSTracingControllerTest, IsRecordingAllowedOTRProtection) {
  auto& instance = IOSTracingController::GetInstance();

  base::TimeTicks now = base::TimeTicks::Now();

  // 1. Without privacy filter enabled, recording is always allowed.
  EXPECT_TRUE(
      IsRecordingAllowed(instance, /*privacy_filter_enabled=*/false, now));

  // 2. With privacy filter enabled:
  // - If no incognito session was ever launched, recording is allowed.
  EXPECT_TRUE(
      IsRecordingAllowed(instance, /*privacy_filter_enabled=*/true, now));

  // - If an incognito session was launched AFTER the tracing session started
  // (session <= incognito),
  //   recording is blocked.
  instance.SetLatestIncognitoLaunchedForTesting(now + base::Seconds(5));
  EXPECT_FALSE(
      IsRecordingAllowed(instance, /*privacy_filter_enabled=*/true, now));

  // - If an incognito session was launched BEFORE the tracing session started
  // (session > incognito),
  //   recording is allowed again.
  instance.SetLatestIncognitoLaunchedForTesting(now - base::Seconds(5));
  EXPECT_TRUE(
      IsRecordingAllowed(instance, /*privacy_filter_enabled=*/true, now));
}
