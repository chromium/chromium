// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <thread>

#include "base/files/scoped_temp_dir.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "base/task/common/task_annotator.h"
#include "base/test/test_trace_processor.h"
#include "base/test/trace_test_utils.h"
#include "build/build_config.h"
#include "components/variations/active_field_trials.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer_proto.h"
#include "services/tracing/public/cpp/perfetto/metadata_data_source.h"
#include "services/tracing/public/cpp/perfetto/track_name_recorder.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/chrome_config.gen.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/histogram_samples.gen.h"

#if BUILDFLAG(IS_POSIX)
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/tracing/perfetto_platform.h"
#include "base/tracing/perfetto_task_runner.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/ipc/service_ipc_host.h"  // nogncheck
#include "third_party/perfetto/protos/perfetto/common/tracing_service_state.gen.h"
#endif

namespace content {

namespace {

const char kDetailedDumpMode[] = "detailed";
const char kBackgroundDumpMode[] = "background";

perfetto::protos::gen::ChromiumHistogramSamplesConfig::HistogramSample
MakeHistogramSample(const std::string& name,
                    std::optional<int64_t> min_value = std::nullopt,
                    std::optional<int64_t> max_value = std::nullopt) {
  perfetto::protos::gen::ChromiumHistogramSamplesConfig::HistogramSample sample;
  sample.set_histogram_name(name);
  if (min_value) {
    sample.set_min_value(*min_value);
  }
  if (max_value) {
    sample.set_max_value(*max_value);
  }
  return sample;
}

perfetto::protos::gen::TraceConfig TraceConfigWithHistograms(
    const std::vector<
        perfetto::protos::gen::ChromiumHistogramSamplesConfig::HistogramSample>&
        histograms) {
  auto perfetto_config = base::test::DefaultTraceConfig("-*", false);

  auto* histogram_data_source = perfetto_config.add_data_sources();
  auto* histogram_source_config = histogram_data_source->mutable_config();
  histogram_source_config->set_name(tracing::mojom::kHistogramSampleSourceName);
  histogram_source_config->set_target_buffer(0);

  if (!histograms.empty()) {
    perfetto::protos::gen::ChromiumHistogramSamplesConfig histogram_config;
    for (const auto& histogram : histograms) {
      *histogram_config.add_histograms() = histogram;
    }
    histogram_source_config->set_chromium_histogram_samples_raw(
        histogram_config.SerializeAsString());
  }

  return perfetto_config;
}

constexpr const char* histogram_query =
    "SELECT "
    "EXTRACT_ARG(arg_set_id, \"chrome_histogram_sample.name\") AS name, "
    "EXTRACT_ARG(arg_set_id, \"chrome_histogram_sample.sample\") AS value "
    "FROM slice "
    "WHERE cat = \"disabled-by-default-histogram_samples\"";

perfetto::protos::gen::TraceConfig TraceConfigWithMemoryDumps(
    const char* mode) {
  const std::string trace_event_config_str = base::StringPrintf(
      "{\"record_mode\":\"record-until-full\","
      "\"included_categories\":[\"disabled-by-default-memory-infra\"],"
      "\"excluded_categories\":[\"*\"],\"memory_dump_config\":{"
      "\"allowed_dump_modes\":[\"%s\"],"
      "\"triggers\":[{\"min_time_between_dumps_ms\":10000,"
      "\"mode\":\"%s\",\"type\":\"periodic_interval\"}]}}",
      mode, mode);

  auto perfetto_config = base::test::DefaultTraceConfig(
      "-*,disabled-by-default-memory-infra", false);
  auto* ds = perfetto_config.add_data_sources();
  ds->mutable_config()->set_name("org.chromium.memory_instrumentation");
  ds->mutable_config()->mutable_chrome_config()->set_trace_config(
      trace_event_config_str);
  return perfetto_config;
}

perfetto::protos::gen::TraceConfig TraceConfigWithMetadata(
    const std::string& category_filter_string) {
  auto perfetto_config =
      base::test::DefaultTraceConfig(category_filter_string, false);

  auto* data_source = perfetto_config.add_data_sources();
  auto* source_config = data_source->mutable_config();
  source_config->set_name("org.chromium.trace_metadata2");

  return perfetto_config;
}

perfetto::protos::gen::TraceConfig TraceConfigWithSamplerProfiler() {
  auto perfetto_config = base::test::DefaultTraceConfig(
      "-*,disabled-by-default-cpu_profiler", false);

  auto* data_source = perfetto_config.add_data_sources();
  auto* source_config = data_source->mutable_config();
  source_config->set_name("org.chromium.sampler_profiler");

  return perfetto_config;
}

perfetto::protos::gen::TraceConfig TraceConfigWithMetadataMultisession(
    const std::string& category_filter_string) {
  auto perfetto_config =
      base::test::DefaultTraceConfig(category_filter_string, false);

  auto* data_source = perfetto_config.add_data_sources();
  auto* source_config = data_source->mutable_config();
  source_config->set_name("org.chromium.trace_metadata2");

  return perfetto_config;
}
}  // namespace

class TracingEndToEndBrowserTest : public ContentBrowserTest {};

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest, SimpleTraceEvent) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace(base::test::DefaultTraceConfig("foo", false),
                 perfetto::kCustomBackend);

  {
    // A simple trace event
    TRACE_EVENT("foo", "test_event");
  }

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query = "SELECT name FROM slice WHERE cat = 'foo'";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name"},
                                     std::vector<std::string>{"test_event"}));
}

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest, Metadata) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace(TraceConfigWithMetadata("-*"));

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result = ttp.RunQuery(
      "SELECT str_value IS NOT NULL AS has_os_name "
      "FROM metadata WHERE name = 'cr-os-name'");
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"has_os_name"},
                                     std::vector<std::string>{"1"}));

  result = ttp.RunQuery(
      "SELECT str_value IS NOT NULL AS has_revision "
      "FROM metadata WHERE name = 'cr-revision'");
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"has_revision"},
                                     std::vector<std::string>{"1"}));

  result = ttp.RunQuery(
      "SELECT int_value > 0 AS has_num_cpus "
      "FROM metadata WHERE name = 'cr-cpu-num-cores'");
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"has_num_cpus"},
                                     std::vector<std::string>{"1"}));
}

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest, MetadataMultisession) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace(TraceConfigWithMetadataMultisession("-*"));

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  base::expected<base::test::TestTraceProcessor::QueryResult,
    std::string> result;

  std::vector<variations::ActiveGroupId> active_group_ids;
  variations::GetFieldTrialActiveGroupIds(std::string_view(),
                                            &active_group_ids);
  if (!active_group_ids.empty()) {
    result = ttp.RunQuery(R"(
      SELECT
        str_value IS NOT NULL AS has_field_trial_hashes
      FROM metadata
      WHERE name = 'cr-a-field_trial_hashes'
    )");
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_THAT(
        result.value(),
        ::testing::ElementsAre(std::vector<std::string>{"has_field_trial_hashes"},
                               std::vector<std::string>{"1"}));
  }

#if BUILDFLAG(IS_ANDROID) && defined(OFFICIAL_BUILD)
  result = ttp.RunQuery(R"(
    SELECT
      int_value IS NOT NULL AS has_version_code
    FROM metadata
    WHERE name = 'cr-a-playstore_version_code'
  )");
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(), ::testing::ElementsAre(
                                  std::vector<std::string>{"has_version_code"},
                                  std::vector<std::string>{"1"}));
#endif
}

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest, TaskExecutionEvent) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("toplevel");

  {
    base::TaskAnnotator task_annotator;
    base::PendingTask task;
    task.task = base::DoNothing();
    task.posted_from = base::Location::CreateForTesting(
        "my_func", "my_file", 0, /*program_counter=*/&task);
    // TaskAnnotator::RunTask is responsible for emitting the task execution
    // event.
    task_annotator.RunTask("RunTaskForTesting", task);
  }

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query =
      "SELECT "
      "EXTRACT_ARG(arg_set_id, 'task.posted_from.file_name') AS file_name, "
      "EXTRACT_ARG(arg_set_id, 'task.posted_from.function_name') AS func_name "
      "FROM slice WHERE cat = 'toplevel' AND name = 'RunTaskForTesting'";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_THAT(
      result.value(),
      ::testing::ElementsAre(std::vector<std::string>{"file_name", "func_name"},
                             std::vector<std::string>{"my_file", "my_func"}));
}

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest, ThreadAndProcessName) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("foo");

  {
    base::PlatformThread::SetName("FooThread");
    TRACE_EVENT("foo", "test_event");
  }

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query =
      "SELECT "
      "thread.name AS thread_name, "
      "process.name AS process_name "
      "FROM slice "
      "JOIN thread_track ON thread_track.id = slice.track_id "
      "JOIN thread USING (utid) "
      "JOIN process USING (upid) "
      "WHERE slice.cat = 'foo'";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(
                  std::vector<std::string>{"thread_name", "process_name"},
                  std::vector<std::string>{"FooThread", "Browser"}));
}

#if defined(TEST_TRACE_PROCESSOR_ENABLED)
// This test checks that TestTraceProcessor links against the correct version of
// SQLite (sqlite_dev). This is important because the version of SQLite that
// is shipped with regular Chrome does not support certain features (e.g. window
// functions).
IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest, CorrectSqliteVersion) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace(base::test::DefaultTraceConfig("foo", false));

  {
    TRACE_EVENT_INSTANT("foo", "event1");
    TRACE_EVENT_INSTANT("foo", "event1");
    TRACE_EVENT_INSTANT("foo", "event2");
  }

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result = ttp.RunQuery(R"(
    SELECT
      name,
      count() OVER (PARTITION BY name) AS same_name_count
    FROM slice
    WHERE category = 'foo'
  )");
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(
                  std::vector<std::string>{"name", "same_name_count"},
                  std::vector<std::string>{"event1", "2"},
                  std::vector<std::string>{"event1", "2"},
                  std::vector<std::string>{"event2", "1"}));
}

#endif  // defined(TEST_TRACE_PROCESSOR_ENABLED)

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest,
                       MemoryInstrumentationBackground) {
  base::WaitableEvent dump_completed;
  memory_instrumentation::TracingObserverProto::GetInstance()
      ->SetOnChromeDumpCallbackForTesting(
          base::BindOnce([](base::WaitableEvent* event) { event->Signal(); },
                         &dump_completed));

  base::test::TestTraceProcessor ttp;
  ttp.StartTrace(TraceConfigWithMemoryDumps(kBackgroundDumpMode));

  while (!dump_completed.IsSignaled()) {
    base::RunLoop().RunUntilIdle();
  }

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result = ttp.RunQuery("SELECT detail_level FROM memory_snapshot");
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(
      result.value(),
      ::testing::ElementsAre(std::vector<std::string>{"detail_level"},
                             std::vector<std::string>{kBackgroundDumpMode}));

  result = ttp.RunQuery(
      "SELECT COUNT(DISTINCT process_snapshot_id) > 1 AS has_other_processes "
      "FROM memory_snapshot_node");
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(
      result.value(),
      ::testing::ElementsAre(std::vector<std::string>{"has_other_processes"},
                             std::vector<std::string>{"1"}));
}

namespace {

class FakeUnwinder : public base::Unwinder {
 public:
  bool CanUnwindFrom(const base::Frame& current_frame) const override {
    return true;
  }

  base::UnwindResult TryUnwind(base::UnwinderStateCapture* capture_state,
                               base::RegisterContext* thread_context,
                               uintptr_t stack_top,
                               std::vector<base::Frame>* stack) override {
    return base::UnwindResult::kCompleted;
  }
};

// Note that this is relevant only for Android, since TracingSamplingProfiler
// ignores any provided unwinder factory for non-Android platforms:
// https://source.chromium.org/chromium/chromium/src/+/main:services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.cc;l=905-908;drc=70d839a3b8bcf1ef43c42a54a4b27f14ee149750
base::StackSamplingProfiler::UnwindersFactory MakeFakeUnwinder() {
  return base::BindOnce([] {
    auto fake_unwinder = std::make_unique<FakeUnwinder>();
    std::vector<std::unique_ptr<base::Unwinder>> unwinders;
    unwinders.push_back(std::move(fake_unwinder));
    return unwinders;
  });
}

}  // namespace

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest, CpuProfiler) {
  // In the browser process, the tracing sampler profiler gets constructed by
  // the chrome/ layer, so we need to do the same manually for testing purposes.
  std::unique_ptr<tracing::TracingSamplerProfiler> tracing_sampler_profiler;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    tracing_sampler_profiler =
        tracing::TracingSamplerProfiler::CreateOnMainThread(
            base::BindRepeating(&MakeFakeUnwinder));
  }

  // There won't be any samples if stack unwinding isn't supported.
  if (!tracing::TracingSamplerProfiler::IsStackUnwindingSupportedForTesting()) {
    GTEST_SKIP() << "Stack unwinding not supported on this platform";
  }

  base::RunLoop wait_for_sample;
  tracing_sampler_profiler->SetSampleCallbackForTesting(
      wait_for_sample.QuitClosure());

  base::test::TestTraceProcessor ttp;
  ttp.StartTrace(TraceConfigWithSamplerProfiler());

  wait_for_sample.Run();

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result = ttp.RunQuery("SELECT * FROM cpu_profile_stack_sample");
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_GT(result.value().size(), 1U);
}

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest,
                       MemoryInstrumentationDetailed) {
  base::WaitableEvent dump_completed;
  memory_instrumentation::TracingObserverProto::GetInstance()
      ->SetOnChromeDumpCallbackForTesting(
          base::BindOnce([](base::WaitableEvent* event) { event->Signal(); },
                         &dump_completed));

  base::test::TestTraceProcessor ttp;
  ttp.StartTrace(TraceConfigWithMemoryDumps(kDetailedDumpMode));

  while (!dump_completed.IsSignaled()) {
    base::RunLoop().RunUntilIdle();
  }

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result = ttp.RunQuery("SELECT detail_level FROM memory_snapshot");
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(), ::testing::ElementsAre(
                                  std::vector<std::string>{"detail_level"},
                                  std::vector<std::string>{kDetailedDumpMode}));

  result = ttp.RunQuery(
      "SELECT COUNT(DISTINCT process_snapshot_id) > 1 AS has_other_processes "
      "FROM memory_snapshot_node");
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(
      result.value(),
      ::testing::ElementsAre(std::vector<std::string>{"has_other_processes"},
                             std::vector<std::string>{"1"}));
}

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest,
                       PackageNameRecordedTraceLogSet) {
  tracing::TrackNameRecorder::SetRecordHostAppPackageName(true);
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace(base::test::DefaultTraceConfig("foo", false),
                 perfetto::kCustomBackend);

  {
    // A simple trace event
    TRACE_EVENT("foo", "test_event");
  }

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query =
      "SELECT "
      "DISTINCT(EXTRACT_ARG(arg_set_id, \"chrome.host_app_package_name\")) "
      "AS name "
      "FROM process "
      "WHERE "
      "EXTRACT_ARG(arg_set_id, \"chrome.host_app_package_name\") IS NOT NULL";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_THAT(
      result.value(),
      ::testing::ElementsAre(
          std::vector<std::string>{"name"},
          std::vector<std::string>{"org.chromium.content_browsertests_apk"}));
}

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest,
                       PackageNameNotRecordedTraceLogNotSet) {
  tracing::TrackNameRecorder::SetRecordHostAppPackageName(false);
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace(base::test::DefaultTraceConfig("foo", false),
                 perfetto::kCustomBackend);

  {
    // A simple trace event
    TRACE_EVENT("foo", "test_event");
  }

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query =
      "SELECT "
      "DISTINCT(EXTRACT_ARG(arg_set_id, \"chrome.host_app_package_name\")) "
      "AS name "
      "FROM process "
      "WHERE "
      "EXTRACT_ARG(arg_set_id, \"chrome.host_app_package_name\") IS NOT NULL";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name"}));
}
#endif  // BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest, TwoSessionsSimple) {
  base::test::TestTraceProcessor ttp1, ttp2;
  ttp1.StartTrace("foo,cat");
  ttp2.StartTrace("test,cat");

  {
    TRACE_EVENT("foo", "foo_event");
    TRACE_EVENT("test", "test_event");
    TRACE_EVENT("cat", "cat_event");
  }

  absl::Status status = ttp1.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query =
      "SELECT name from slice WHERE cat IN ('foo', 'test', 'cat') ORDER BY ts";
  auto result1 = ttp1.RunQuery(query);
  ASSERT_TRUE(result1.has_value()) << result1.error();
  EXPECT_THAT(result1.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name"},
                                     std::vector<std::string>{"foo_event"},
                                     std::vector<std::string>{"cat_event"}));

  status = ttp2.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result2 = ttp2.RunQuery(query);
  ASSERT_TRUE(result2.has_value()) << result2.error();
  EXPECT_THAT(result2.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name"},
                                     std::vector<std::string>{"test_event"},
                                     std::vector<std::string>{"cat_event"}));
}

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest,
                       TwoSessionsGlobalHistograms) {
  ASSERT_FALSE(base::StatisticsRecorder::global_sample_callback());

  // Start the first session that records "Test.Foo" histograms
  auto config = TraceConfigWithHistograms({});
  base::test::TestTraceProcessor ttp1;
  ttp1.StartTrace(config);

  base::UmaHistogramCounts100("Test.Foo", 1);
  base::UmaHistogramCounts100("Test.Bar", 2);

  // Start the second session that records all histograms
  config = TraceConfigWithHistograms({});
  base::test::TestTraceProcessor ttp2;
  ttp2.StartTrace(config);

  base::UmaHistogramCounts100("Test.Foo", 3);
  base::UmaHistogramCounts100("Test.Bar", 4);

  // Stop the second session.
  absl::Status status = ttp2.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result = ttp2.RunQuery(histogram_query);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(), ::testing::IsSupersetOf(
                                  {std::vector<std::string>{"name", "value"},
                                   std::vector<std::string>{"Test.Foo", "3"},
                                   std::vector<std::string>{"Test.Bar", "4"}}));

  base::UmaHistogramCounts100("Test.Foo", 5);
  base::UmaHistogramCounts100("Test.Bar", 6);

  // Stop the first session. Its trace should contain all samples that were
  // recorded while the session was active.
  status = ttp1.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  ASSERT_FALSE(base::StatisticsRecorder::global_sample_callback());

  result = ttp1.RunQuery(histogram_query);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(), ::testing::IsSupersetOf(
                                  {std::vector<std::string>{"name", "value"},
                                   std::vector<std::string>{"Test.Foo", "1"},
                                   std::vector<std::string>{"Test.Bar", "2"},
                                   std::vector<std::string>{"Test.Foo", "3"},
                                   std::vector<std::string>{"Test.Bar", "4"},
                                   std::vector<std::string>{"Test.Foo", "5"},
                                   std::vector<std::string>{"Test.Bar", "6"}}));
}

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest,
                       TwoSessionsTargetedHistograms) {
  // Start the first session that records "Test.Foo" histograms
  auto config =
      TraceConfigWithHistograms({MakeHistogramSample("Test.Foo", 10, 20)});
  base::test::TestTraceProcessor ttp1;
  ttp1.StartTrace(config);

  base::UmaHistogramCounts100("Test.Foo", 1);  // Not recorded
  base::UmaHistogramCounts100("Test.Foo", 10);
  base::UmaHistogramCounts100("Test.Foo", 21);  // Not recorded

  // Start the second session that records "Test.Bar" histograms
  config = TraceConfigWithHistograms({MakeHistogramSample("Test.Bar", 30, 40)});
  base::test::TestTraceProcessor ttp2;
  ttp2.StartTrace(config);

  base::UmaHistogramCounts100("Test.Foo", 11);
  base::UmaHistogramCounts100("Test.Bar", 15);  // Not recorded
  base::UmaHistogramCounts100("Test.Bar", 30);

  // Stop the second session. Its trace should contain only one "Test.Bar"
  // sample that was recorded while the session was active.
  absl::Status status = ttp2.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result = ttp2.RunQuery(histogram_query);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(), ::testing::ElementsAre(
                                  std::vector<std::string>{"name", "value"},
                                  std::vector<std::string>{"Test.Bar", "30"}));

  base::UmaHistogramCounts100("Test.Foo", 12);
  base::UmaHistogramCounts100("Test.Bar", 31);  // Not recorded

  // Stop the first session. Its trace should contain all three "Test.Foo"
  // samples that were recorded while the session was active.
  status = ttp1.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  result = ttp1.RunQuery(histogram_query);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(), ::testing::ElementsAre(
                                  std::vector<std::string>{"name", "value"},
                                  std::vector<std::string>{"Test.Foo", "10"},
                                  std::vector<std::string>{"Test.Foo", "11"},
                                  std::vector<std::string>{"Test.Foo", "12"}));
}

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest, TwoSessionsProcessNames) {
  base::test::TestTraceProcessor ttp1, ttp2;
  ttp1.StartTrace("foo");
  ttp2.StartTrace("test");

  {
    TRACE_EVENT("foo", "foo_event");
    TRACE_EVENT("test", "test_event");
  }

  absl::Status status = ttp1.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query =
      "SELECT COUNT(*) AS cnt FROM process WHERE name = \"Browser\"";
  auto result1 = ttp1.RunQuery(query);
  ASSERT_TRUE(result1.has_value()) << result1.error();
  EXPECT_THAT(result1.value(),
              ::testing::ElementsAre(std::vector<std::string>{"cnt"},
                                     std::vector<std::string>{"1"}));

  status = ttp2.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result2 = ttp2.RunQuery(query);
  ASSERT_TRUE(result2.has_value()) << result2.error();
  EXPECT_THAT(result2.value(),
              ::testing::ElementsAre(std::vector<std::string>{"cnt"},
                                     std::vector<std::string>{"1"}));
}

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest,
                       TwoSessionsMemoryInstrumentation) {
  base::WaitableEvent dump_completed;
  memory_instrumentation::TracingObserverProto::GetInstance()
      ->SetOnChromeDumpCallbackForTesting(
          base::BindOnce([](base::WaitableEvent* event) { event->Signal(); },
                         &dump_completed));

  base::test::TestTraceProcessor ttp1, ttp2;
  ttp1.StartTrace(TraceConfigWithMemoryDumps(kDetailedDumpMode));
  ttp2.StartTrace("foo");

  {
    TRACE_EVENT("foo", "foo_event");
    while (!dump_completed.IsSignaled()) {
      base::RunLoop().RunUntilIdle();
    }
  }

  absl::Status status = ttp1.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query = "SELECT detail_level FROM memory_snapshot";
  auto result1 = ttp1.RunQuery(query);
  ASSERT_TRUE(result1.has_value()) << result1.error();
  EXPECT_THAT(
      result1.value(),
      ::testing::ElementsAre(std::vector<std::string>{"detail_level"},
                             std::vector<std::string>{kDetailedDumpMode}));

  status = ttp2.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result2 = ttp2.RunQuery(query);
  ASSERT_TRUE(result2.has_value()) << result2.error();
  EXPECT_THAT(result2.value(),
              ::testing::ElementsAre(std::vector<std::string>{"detail_level"}));
}

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest, TwoSessionsMetadata) {
  base::test::TestTraceProcessor ttp1, ttp2;
  ttp1.StartTrace(TraceConfigWithMetadataMultisession("-*"));
  ttp2.StartTrace(TraceConfigWithMetadataMultisession("-*"));

  absl::Status status = ttp1.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  base::expected<base::test::TestTraceProcessor::QueryResult,
    std::string> result;

  std::vector<variations::ActiveGroupId> active_group_ids;
  variations::GetFieldTrialActiveGroupIds(std::string_view(),
                                            &active_group_ids);

  if (!active_group_ids.empty()) {
    result = ttp1.RunQuery(R"(
      SELECT
        str_value IS NOT NULL AS has_field_trial_hashes
      FROM metadata
      WHERE name = 'cr-a-field_trial_hashes'
    )");
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_THAT(
        result.value(),
        ::testing::ElementsAre(std::vector<std::string>{"has_field_trial_hashes"},
                               std::vector<std::string>{"1"}));
  }

#if BUILDFLAG(IS_ANDROID) && defined(OFFICIAL_BUILD)
  result = ttp1.RunQuery(R"(
    SELECT
      int_value IS NOT NULL AS has_version_code
    FROM metadata
    WHERE name = 'cr-a-playstore_version_code'
  )");
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(), ::testing::ElementsAre(
                                  std::vector<std::string>{"has_version_code"},
                                  std::vector<std::string>{"1"}));
#endif

  status = ttp2.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  if (!active_group_ids.empty()) {
    result = ttp2.RunQuery(R"(
      SELECT
        str_value IS NOT NULL AS has_field_trial_hashes
      FROM metadata
      WHERE name = 'cr-a-field_trial_hashes'
    )");
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_THAT(
        result.value(),
        ::testing::ElementsAre(std::vector<std::string>{"has_field_trial_hashes"},
                               std::vector<std::string>{"1"}));
  }

#if BUILDFLAG(IS_ANDROID) && defined(OFFICIAL_BUILD)
  result = ttp2.RunQuery(R"(
    SELECT
      int_value IS NOT NULL AS has_version_code
    FROM metadata
    WHERE name = 'cr-a-playstore_version_code'
  )");
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(), ::testing::ElementsAre(
                                  std::vector<std::string>{"has_version_code"},
                                  std::vector<std::string>{"1"}));
#endif
}

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest, AddTraceEventWithProcessId) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace(base::test::DefaultTraceConfig("memory", false));

  const base::ProcessId browser_pid = base::GetCurrentProcId();

  std::vector<base::ProcessId> child_pids;
  content::BrowserChildProcessHostIterator iterator;
  while (!iterator.Done()) {
    const content::ChildProcessData& data = iterator.GetData();
    if (data.GetProcess().IsValid()) {
      child_pids.push_back(data.GetProcess().Pid());
    }
    ++iterator;
  }

  const unsigned char* category_group_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("memory");

  TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_PROCESS_ID(
      TRACE_EVENT_PHASE_INSTANT, category_group_enabled, "foo_memory_event", 0,
      browser_pid, nullptr, TRACE_EVENT_FLAG_HAS_ID);

  for (base::ProcessId child_pid : child_pids) {
    TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_PROCESS_ID(
        TRACE_EVENT_PHASE_INSTANT, category_group_enabled, "foo_memory_event",
        0, child_pid, nullptr, TRACE_EVENT_FLAG_HAS_ID);
  }

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query = R"(
    SELECT
      slice.name AS event_name,
      process.pid AS pid
    FROM slice
    JOIN thread_track ON slice.track_id = thread_track.id
    JOIN thread ON thread_track.utid = thread.utid
    JOIN process ON thread.upid = process.upid
    WHERE slice.name = 'foo_memory_event'
    )";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();

  size_t expected_row_count = 2 + child_pids.size();
  const auto& rows = result.value();
  EXPECT_EQ(rows.size(), expected_row_count);

  EXPECT_THAT(rows[0], ::testing::ElementsAre("event_name", "pid"));
  EXPECT_THAT(rows[1],
              ::testing::ElementsAre("foo_memory_event",
                                     base::NumberToString(browser_pid)));

  for (size_t i = 0; i < child_pids.size(); ++i) {
    EXPECT_THAT(rows[i + 2],
                ::testing::ElementsAre("foo_memory_event",
                                       base::NumberToString(child_pids[i])));
  }
}

#if BUILDFLAG(IS_POSIX)
class SystemTracingEndToEndBrowserTest : public ContentBrowserTest {
 public:
  void SetUp() override {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ASSERT_EQ(0, setenv("PERFETTO_PRODUCER_SOCK_NAME",
                        temp_dir_.GetPath()
                            .Append(FILE_PATH_LITERAL("producer"))
                            .value()
                            .c_str(),
                        /*overwrite=*/true));
    ASSERT_EQ(0, setenv("PERFETTO_CONSUMER_SOCK_NAME",
                        temp_dir_.GetPath()
                            .Append(FILE_PATH_LITERAL("consumer"))
                            .value()
                            .c_str(),
                        /*overwrite=*/true));
    feature_list_.InitAndEnableFeature(features::kEnablePerfettoSystemTracing);
    tracing::PerfettoTracedProcess::SetAllowSystemTracingConsumerForTesting(
        true);

    ContentBrowserTest::SetUp();
  }

  void PreRunTestOnMainThread() override {
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
    base::RunLoop system_service_creation;

    task_runner_->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([this]() {
          perfetto_task_runner_ =
              std::make_unique<base::tracing::PerfettoTaskRunner>(
                  task_runner_.get());
          system_service_ = perfetto::ServiceIPCHost::CreateInstance(
              perfetto_task_runner_.get());
          system_service_->Start(perfetto::GetProducerSocket(),
                                 perfetto::GetConsumerSocket());
          system_service_->service()->SetSMBScrapingEnabled(true);
        }),
        system_service_creation.QuitClosure());
    system_service_creation.Run();

    DCHECK(system_service_);
    EXPECT_TRUE(WaitForCurrentProcessConnected());

    ContentBrowserTest::PreRunTestOnMainThread();
  }

  void PostRunTestOnMainThread() override {
    task_runner_->DeleteSoon(FROM_HERE, std::move(system_service_));
    task_runner_->DeleteSoon(FROM_HERE, std::move(perfetto_task_runner_));
    unlink(perfetto::GetProducerSocket());
    unlink(perfetto::GetConsumerSocket());

    ContentBrowserTest::PostRunTestOnMainThread();
  }

  void TearDown() override {
    ASSERT_EQ(0, unsetenv("PERFETTO_PRODUCER_SOCK_NAME"));
    ASSERT_EQ(0, unsetenv("PERFETTO_CONSUMER_SOCK_NAME"));
  }

 private:
  // Waits for the current process to connect to the tracing service as a
  // producer.
  bool WaitForCurrentProcessConnected() {
    std::string current_process_name = tracing::PerfettoTracedProcess::Get()
                                           .perfetto_platform_for_testing()
                                           ->GetCurrentProcessName();
    std::unique_ptr<perfetto::TracingSession> session =
        perfetto::Tracing::NewTrace(perfetto::kSystemBackend);
    for (size_t i = 0; i < 100; i++) {
      auto result = session->QueryServiceStateBlocking();
      perfetto::protos::gen::TracingServiceState state;
      EXPECT_TRUE(result.success);
      EXPECT_TRUE(state.ParseFromArray(result.service_state_data.data(),
                                       result.service_state_data.size()));
      for (const auto& producer : state.producers()) {
        if (producer.name() == current_process_name) {
          return true;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
  }

  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<base::tracing::PerfettoTaskRunner> perfetto_task_runner_;
  std::unique_ptr<perfetto::ServiceIPCHost> system_service_;
};

IN_PROC_BROWSER_TEST_F(SystemTracingEndToEndBrowserTest, SimpleTraceEvent) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace(base::test::DefaultTraceConfig("foo", false),
                 perfetto::kSystemBackend);

  {
    // A simple trace event
    TRACE_EVENT("foo", "test_event");
  }

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query = "SELECT name FROM slice WHERE cat = 'foo'";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name"},
                                     std::vector<std::string>{"test_event"}));
}

// Tests that system tracing works from a sandboxed process (Renderer).
// The test fails on Android because Renderers can't connect to an
// arbitrary socket. Flaky on Mac since the renderer doesn't connect on
// time. crbug.com/324063092
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
#define MAYBE_PerformanceMark DISABLED_PerformanceMark
#else
#define MAYBE_PerformanceMark PerformanceMark
#endif
IN_PROC_BROWSER_TEST_F(SystemTracingEndToEndBrowserTest,
                       MAYBE_PerformanceMark) {
  auto session = perfetto::Tracing::NewTrace(perfetto::kSystemBackend);
  session->Setup(base::test::DefaultTraceConfig("blink.user_timing", false));
  base::RunLoop start_session;
  session->SetOnStartCallback(
      [&start_session]() { start_session.QuitWhenIdle(); });
  session->Start();
  start_session.Run();

  Shell* tab = CreateBrowser();

  // Wait until the renderer connects to the tracing service and starts tracing.
  // We do this by periodically emitting a performance mark and checking the
  // trace contents for its name. This can lead to multiple marks appearing in
  // the trace (e.g. if the renderer does startup tracing for some time before
  // connecting to the service), but it doesn't matter. We just want to make
  // sure that at least one of them is there.
  std::vector<char> trace;
  size_t i = 0;
  for (; i < 300; i++) {
    EXPECT_TRUE(ExecJs(tab, "performance.mark('mark1');"));

    base::RunLoop flush;
    session->Flush([&flush](bool) { flush.QuitWhenIdle(); });
    flush.Run();

    std::vector<char> buffer = session->ReadTraceBlocking();
    trace.insert(trace.end(), buffer.begin(), buffer.end());
    std::vector<char> mark_name = {'m', 'a', 'r', 'k', '1'};
    auto it = std::search(buffer.begin(), buffer.end(), mark_name.begin(),
                          mark_name.end());
    if (it != buffer.end()) {
      break;
    }
  }
  ASSERT_LT(i, 300U);

  base::test::TestTraceProcessorImpl ttp;
  absl::Status status = ttp.ParseTrace(trace);
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query =
      "SELECT name FROM slice "
      "WHERE cat = 'blink.user_timing' AND name = 'mark1' LIMIT 1";
  auto result = ttp.ExecuteQuery(query);
  ASSERT_TRUE(result.ok()) << result.error();

  EXPECT_THAT(result.result(),
              ::testing::ElementsAre(std::vector<std::string>{"name"},
                                     std::vector<std::string>{"mark1"}));
}
#endif  // BUILDFLAG(IS_POSIX)

}  // namespace content
