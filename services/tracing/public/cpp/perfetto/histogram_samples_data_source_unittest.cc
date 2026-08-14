// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/histogram_samples_data_source.h"

#include <optional>
#include <string>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/tracing/test_trace_processor.h"
#include "base/test/tracing/trace_test_utils.h"
#include "services/tracing/public/cpp/perfetto/perfetto_data_source_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/histogram_samples.gen.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.gen.h"

namespace tracing {

namespace {

perfetto::protos::gen::TraceConfig GetTraceConfig(
    std::optional<perfetto::protos::gen::ChromiumHistogramSamplesConfig>
        histogram_samples_config = std::nullopt) {
  perfetto::protos::gen::TraceConfig config;
  auto* buffer_config = config.add_buffers();
  buffer_config->set_size_kb(4 * 1024);

  auto* data_source = config.add_data_sources();
  auto* source_config = data_source->mutable_config();
  source_config->set_name(kHistogramSampleSourceName);
  source_config->set_target_buffer(0);

  if (histogram_samples_config.has_value()) {
    source_config->set_chromium_histogram_samples_raw(
        histogram_samples_config->SerializeAsString());
  }

  return config;
}

}  // namespace

class HistogramSamplesDataSourceTest : public ::testing::Test {
 protected:
  void SetUp() override { HistogramSamplesDataSource::Register(); }

 private:
  base::test::TracingEnvironment tracing_environment_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(HistogramSamplesDataSourceTest, RecordAllHistogramsWithoutTrackEvent) {
  base::test::TestTraceProcessor test_trace_processor;
  test_trace_processor.StartTrace(GetTraceConfig());

  base::UmaHistogramExactLinear("Test.Histogram.All", 42, 100);

  auto status = test_trace_processor.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result = test_trace_processor.RunQuery(R"(
    INCLUDE PERFETTO MODULE chrome.histograms;
    SELECT name, value FROM chrome_histograms WHERE name = 'Test.Histogram.All';
  )");
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(
                  std::vector<std::string>{"name", "value"},
                  std::vector<std::string>{"Test.Histogram.All", "42"}));

  auto missing_uuid_result = test_trace_processor.RunQuery(R"(
    SELECT value FROM stats WHERE name = 'track_hierarchy_missing_uuid';
  )");
  ASSERT_TRUE(missing_uuid_result.has_value()) << missing_uuid_result.error();
  if (missing_uuid_result.value().size() > 1) {
    EXPECT_EQ(missing_uuid_result.value()[1][0], "0");
  }
}

TEST_F(HistogramSamplesDataSourceTest, MultipleHistogramSamples) {
  base::test::TestTraceProcessor test_trace_processor;
  test_trace_processor.StartTrace(GetTraceConfig());

  base::UmaHistogramExactLinear("Test.Histogram.A", 10, 100);
  base::UmaHistogramExactLinear("Test.Histogram.B", 20, 100);
  base::UmaHistogramExactLinear("Test.Histogram.A", 30, 100);

  auto status = test_trace_processor.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result = test_trace_processor.RunQuery(R"(
    INCLUDE PERFETTO MODULE chrome.histograms;
    SELECT name, value FROM chrome_histograms
    WHERE name GLOB 'Test.Histogram.*'
    ORDER BY ts ASC;
  )");
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(
                  std::vector<std::string>{"name", "value"},
                  std::vector<std::string>{"Test.Histogram.A", "10"},
                  std::vector<std::string>{"Test.Histogram.B", "20"},
                  std::vector<std::string>{"Test.Histogram.A", "30"}));
}

TEST_F(HistogramSamplesDataSourceTest, EmitsProcessAndThreadTrackDescriptors) {
  base::test::TestTraceProcessor test_trace_processor;
  test_trace_processor.StartTrace(GetTraceConfig());

  base::UmaHistogramExactLinear("Test.Histogram.TrackInfo", 77, 100);

  auto status = test_trace_processor.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result = test_trace_processor.RunQuery(R"(
    INCLUDE PERFETTO MODULE chrome.histograms;
    SELECT name, value, pid, tid FROM chrome_histograms
    WHERE name = 'Test.Histogram.TrackInfo';
  )");
  ASSERT_TRUE(result.has_value()) << result.error();

  std::string expected_pid = base::NumberToString(
      static_cast<int32_t>(perfetto::ProcessTrack::Current().pid));
  std::string expected_tid = base::NumberToString(
      static_cast<int32_t>(perfetto::ThreadTrack::Current().tid));

  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(
                  std::vector<std::string>{"name", "value", "pid", "tid"},
                  std::vector<std::string>{"Test.Histogram.TrackInfo", "77",
                                           expected_pid, expected_tid}));
}

TEST_F(HistogramSamplesDataSourceTest, FilterHistogramNamesForPrivacy) {
  base::test::TestTraceProcessor test_trace_processor;
  perfetto::protos::gen::ChromiumHistogramSamplesConfig histogram_config;
  histogram_config.set_filter_histogram_names(true);
  test_trace_processor.StartTrace(GetTraceConfig(histogram_config));

  base::UmaHistogramExactLinear("Test.Histogram.Private", 42, 100);

  auto status = test_trace_processor.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result = test_trace_processor.RunQuery(R"(
    INCLUDE PERFETTO MODULE chrome.histograms;
    SELECT name, value FROM chrome_histograms;
  )");
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name", "value"},
                                     std::vector<std::string>{"[NULL]", "42"}));
}

}  // namespace tracing
