// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/tracing_handler.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/trace_event/trace_config.h"
#include "base/values.h"
#include "services/tracing/public/cpp/perfetto/perfetto_data_source_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace protocol {

namespace {

const char kCustomTraceConfigString[] =
    "{"
    "\"enable_argument_filter\":true,"
    "\"enable_package_name_filter\":false,"
    "\"enable_systrace\":true,"
    "\"excluded_categories\":[\"excluded\",\"exc_pattern*\"],"
    "\"included_categories\":[\"included\","
    "\"inc_pattern*\","
    "\"disabled-by-default-cc\","
    "\"disabled-by-default-memory-infra\"],"
    "\"memory_dump_config\":{"
    "\"allowed_dump_modes\":[\"background\",\"light\",\"detailed\"],"
    "\"triggers\":["
    "{"
    "\"min_time_between_dumps_ms\":50,"
    "\"mode\":\"light\","
    "\"type\":\"periodic_interval\""
    "},"
    "{"
    "\"min_time_between_dumps_ms\":1000,"
    "\"mode\":\"detailed\","
    "\"type\":\"periodic_interval\""
    "}"
    "]"
    "},"
    "\"record_mode\":\"record-continuously\","
    "\"trace_buffer_size_in_kb\":262144"
    "}";

const char kCustomTraceConfigStringDevToolsStyle[] =
    "{"
    "\"enableArgumentFilter\":true,"
    "\"enableSystrace\":true,"
    "\"excludedCategories\":[\"excluded\",\"exc_pattern*\"],"
    "\"includedCategories\":[\"included\","
    "\"inc_pattern*\","
    "\"disabled-by-default-cc\","
    "\"disabled-by-default-memory-infra\"],"
    "\"memoryDumpConfig\":{"
    "\"allowedDumpModes\":[\"background\",\"light\",\"detailed\"],"
    "\"triggers\":["
    "{"
    "\"minTimeBetweenDumpsMs\":50,"
    "\"mode\":\"light\","
    "\"type\":\"periodic_interval\""
    "},"
    "{"
    "\"minTimeBetweenDumpsMs\":1000,"
    "\"mode\":\"detailed\","
    "\"type\":\"periodic_interval\""
    "}"
    "]"
    "},"
    "\"recordMode\":\"recordContinuously\","
    "\"traceBufferSizeInKb\":262144"
    "}";

}  // namespace

class TracingHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    tracing_handler_ =
        std::make_unique<TracingHandler>(nullptr, nullptr, nullptr);
  }

  void TearDown() override { tracing_handler_.reset(); }

  std::string GetValidTraceFragment(const std::string& trace_fragment) {
    const std::string valid_trace_fragment =
        tracing_handler_->UpdateTraceDataBuffer(trace_fragment);
    return valid_trace_fragment.substr(
        tracing_handler_->trace_data_buffer_state_.offset);
  }

 private:
  std::unique_ptr<TracingHandler> tracing_handler_;
};

TEST_F(TracingHandlerTest, GetTraceConfigFromDevToolsConfig) {
  base::Value devtools_config =
      base::JSONReader::Read(kCustomTraceConfigStringDevToolsStyle,
                             base::JSON_PARSE_CHROMIUM_EXTENSIONS)
          .value();

  base::trace_event::TraceConfig trace_config =
      TracingHandler::GetTraceConfigFromDevToolsConfig(devtools_config);

  EXPECT_STREQ(kCustomTraceConfigString, trace_config.ToString().c_str());
}

TEST_F(TracingHandlerTest, SimpleGetValidTraceFragment) {
  // No prefix is valid.
  EXPECT_EQ("", GetValidTraceFragment("{pid: 1, "));

  // The longest valid prefix of "{pid: 1, args: {}}, {pid: 2" is
  // "{pid: 1, args: {}}".
  EXPECT_EQ("{pid: 1, args: {}}", GetValidTraceFragment("args: {}}, {pid: 2"));

  EXPECT_EQ("{pid: 2}, {pid: 3}", GetValidTraceFragment("}, {pid: 3}"));
}

TEST_F(TracingHandlerTest, GetValidTraceFragmentBreakBeforeComma) {
  EXPECT_EQ("{pid: 1}", GetValidTraceFragment("{pid: 1}"));
  // The comma should be ignored.
  EXPECT_EQ("{pid: 2}", GetValidTraceFragment(",{pid: 2}"));
}

TEST_F(TracingHandlerTest, ComplexGetValidTraceFragment) {
  const std::string chunk1 = "{\"pid\":1,\"args\":{\"key\":\"}\"},\"tid\":1}";
  const std::string chunk2 =
      "{\"pid\":2,\"args\":{\"key\":{\"key\":\"\\\"t}\"},\"key2\":2},\"tid\":"
      "2}";
  const std::string trace_data = chunk1 + "," + chunk2;

  EXPECT_EQ("", GetValidTraceFragment(trace_data.substr(0, chunk1.size() - 1)));
  EXPECT_EQ(chunk1, GetValidTraceFragment(trace_data.substr(
                        chunk1.size() - 1, trace_data.size() - chunk1.size())));
  EXPECT_EQ(chunk2,
            GetValidTraceFragment(trace_data.substr(trace_data.size() - 1, 1)));
}

TEST_F(TracingHandlerTest, ProcessFilterClearsRegex) {
  perfetto::TraceConfig trace_config;

  auto* data_source = trace_config.add_data_sources();
  auto* config = data_source->mutable_config();
  config->set_name("track_event");
  data_source->add_producer_name_regex_filter(".*");
  data_source->add_producer_name_filter("old_filter");

  auto* data_source2 = trace_config.add_data_sources();
  auto* config2 = data_source2->mutable_config();
  config2->set_name("org.chromium.foo");
  data_source2->add_producer_name_regex_filter(".*");

  auto* data_source3 = trace_config.add_data_sources();
  auto* config3 = data_source3->mutable_config();
  config3->set_name("other_source");
  data_source3->add_producer_name_regex_filter(".*");

  std::unordered_set<base::ProcessId> pids = {1234};
  TracingHandler::AddPidsToProcessFilter(pids, trace_config);

  ASSERT_EQ(3, trace_config.data_sources_size());

  // Chrome track_event data source: regex cleared, pid added
  EXPECT_EQ("track_event", trace_config.data_sources()[0].config().name());
  EXPECT_EQ(0, trace_config.data_sources()[0].producer_name_regex_filter_size());
  ASSERT_EQ(1, trace_config.data_sources()[0].producer_name_filter_size());
  EXPECT_EQ(std::string(tracing::kPerfettoProducerNamePrefix) + "1234",
            trace_config.data_sources()[0].producer_name_filter()[0]);

  // Chrome data source: regex cleared, pid added
  EXPECT_EQ("org.chromium.foo", trace_config.data_sources()[1].config().name());
  EXPECT_EQ(0, trace_config.data_sources()[1].producer_name_regex_filter_size());
  ASSERT_EQ(1, trace_config.data_sources()[1].producer_name_filter_size());
  EXPECT_EQ(std::string(tracing::kPerfettoProducerNamePrefix) + "1234",
            trace_config.data_sources()[1].producer_name_filter()[0]);

  // Other data source: regex retained, pid not added
  EXPECT_EQ("other_source", trace_config.data_sources()[2].config().name());
  ASSERT_EQ(1, trace_config.data_sources()[2].producer_name_regex_filter_size());
  EXPECT_EQ(".*", trace_config.data_sources()[2].producer_name_regex_filter()[0]);
  EXPECT_EQ(0, trace_config.data_sources()[2].producer_name_filter_size());
}

TEST_F(TracingHandlerTest, ProcessFilterAppendsPids) {
  perfetto::TraceConfig trace_config;

  auto* data_source = trace_config.add_data_sources();
  auto* config = data_source->mutable_config();
  config->set_name("track_event");

  // Initial PIDs
  std::unordered_set<base::ProcessId> pids1 = {1234, 5678};
  TracingHandler::AddPidsToProcessFilter(pids1, trace_config);

  ASSERT_EQ(1, trace_config.data_sources_size());
  EXPECT_EQ(2, trace_config.data_sources()[0].producer_name_filter_size());

  // Append new PID
  std::unordered_set<base::ProcessId> pids2 = {9012};
  TracingHandler::AddPidsToProcessFilter(pids2, trace_config);

  ASSERT_EQ(1, trace_config.data_sources_size());
  // Should have 3 PIDs now
  EXPECT_EQ(3, trace_config.data_sources()[0].producer_name_filter_size());

  EXPECT_THAT(trace_config.data_sources()[0].producer_name_filter(),
              testing::Contains(
                  std::string(tracing::kPerfettoProducerNamePrefix) + "1234"));
  EXPECT_THAT(trace_config.data_sources()[0].producer_name_filter(),
              testing::Contains(
                  std::string(tracing::kPerfettoProducerNamePrefix) + "5678"));
  EXPECT_THAT(trace_config.data_sources()[0].producer_name_filter(),
              testing::Contains(
                  std::string(tracing::kPerfettoProducerNamePrefix) + "9012"));
}


}  // namespace protocol
}  // namespace content
