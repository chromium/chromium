// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tracing/ios_tracing_controller.h"

#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/run_loop.h"
#import "base/strings/string_number_conversions.h"
#import "base/task/thread_pool.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/scoped_command_line.h"
#import "base/test/task_environment.h"
#import "base/test/test_trace_processor.h"
#import "base/trace_event/trace_event.h"
#import "base/trace_event/trace_log.h"
#import "base/tracing/perfetto_platform.h"
#import "components/tracing/common/tracing_switches.h"
#import "services/tracing/public/cpp/perfetto/perfetto_data_source_names.h"
#import "services/tracing/public/cpp/startup_tracing_controller.h"
#import "services/tracing/public/cpp/trace_startup_config.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/perfetto/protos/perfetto/config/chrome/chrome_config.gen.h"
#import "third_party/perfetto/protos/perfetto/config/data_source_config.gen.h"
#import "third_party/perfetto/protos/perfetto/config/trace_config.gen.h"

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
