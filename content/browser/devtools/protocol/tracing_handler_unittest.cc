// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/tracing_handler.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_config.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/devtools_traceable_screenshot.h"
#include "services/tracing/public/cpp/perfetto/perfetto_data_source_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

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

class StubClient : public DevToolsAgentHostClient {
 public:
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {}
  void AgentHostClosed(DevToolsAgentHost* agent_host) override {}
};

class TracingHandlerTest : public testing::Test,
                           public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    client_ = std::make_unique<StubClient>();
    session_ = std::make_unique<DevToolsSession>(
        client_.get(), DevToolsSession::Mode::kSupportsTabTarget);
    tracing_handler_ =
        std::make_unique<TracingHandler>(nullptr, nullptr, session_.get(),
                                         /*is_trusted=*/GetParam());
  }

  void TearDown() override { tracing_handler_.reset(); }

  std::string GetValidTraceFragment(const std::string& trace_fragment) {
    const std::string valid_trace_fragment =
        tracing_handler_->UpdateTraceDataBuffer(trace_fragment);
    return valid_trace_fragment.substr(
        tracing_handler_->trace_data_buffer_state_.offset);
  }

  perfetto::TraceConfig GetTraceConfig() {
    return tracing_handler_->trace_config_;
  }

  void SetDidInitiateRecording(bool value) {
    tracing_handler_->did_initiate_recording_ = value;
  }

  void CallAddProcessToFilter(base::ProcessId pid) {
    tracing_handler_->AddProcessToFilter(pid);
  }

  static void CallAddPidsToProcessFilter(
      const std::unordered_set<base::ProcessId>& pids,
      perfetto::TraceConfig& config) {
    TracingHandler::AddPidsToProcessFilter(pids, config);
  }

  TracingHandler* tracing_handler() { return tracing_handler_.get(); }

 protected:
  std::unique_ptr<StubClient> client_;
  std::unique_ptr<DevToolsSession> session_;
  std::unique_ptr<TracingHandler> tracing_handler_;
};

TEST_P(TracingHandlerTest, GetTraceConfigFromDevToolsConfig) {
  base::Value devtools_config =
      base::JSONReader::Read(kCustomTraceConfigStringDevToolsStyle,
                             base::JSON_PARSE_CHROMIUM_EXTENSIONS)
          .value();

  base::trace_event::TraceConfig trace_config =
      TracingHandler::GetTraceConfigFromDevToolsConfig(devtools_config);

  EXPECT_STREQ(kCustomTraceConfigString, trace_config.ToString().c_str());
}

TEST_P(TracingHandlerTest, SimpleGetValidTraceFragment) {
  // No prefix is valid.
  EXPECT_EQ("", GetValidTraceFragment("{pid: 1, "));

  // The longest valid prefix of "{pid: 1, args: {}}, {pid: 2" is
  // "{pid: 1, args: {}}".
  EXPECT_EQ("{pid: 1, args: {}}", GetValidTraceFragment("args: {}}, {pid: 2"));

  EXPECT_EQ("{pid: 2}, {pid: 3}", GetValidTraceFragment("}, {pid: 3}"));
}

TEST_P(TracingHandlerTest, GetValidTraceFragmentBreakBeforeComma) {
  EXPECT_EQ("{pid: 1}", GetValidTraceFragment("{pid: 1}"));
  // The comma should be ignored.
  EXPECT_EQ("{pid: 2}", GetValidTraceFragment(",{pid: 2}"));
}

TEST_P(TracingHandlerTest, ComplexGetValidTraceFragment) {
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

TEST_P(TracingHandlerTest, ProcessFilterAppendsPids) {
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

TEST_P(TracingHandlerTest, ResolveScreenshotParams) {
  constexpr int kDefaultSize = 500;
  constexpr int kDefaultCount =
      DevToolsTraceableScreenshot::kDefaultMaximumNumberOfScreenshots;

  // Defaults are applied when no values are supplied.
  {
    gfx::Size size;
    int count = 0;
    EXPECT_TRUE(TracingHandler::ResolveScreenshotParams(
                    std::nullopt, std::nullopt, &size, &count)
                    .IsSuccess());
    EXPECT_EQ(gfx::Size(kDefaultSize, kDefaultSize), size);
    EXPECT_EQ(kDefaultCount, count);
  }

  // Explicit equivalent of the defaults is accepted.
  {
    gfx::Size size;
    int count = 0;
    EXPECT_TRUE(TracingHandler::ResolveScreenshotParams(
                    kDefaultSize, kDefaultCount, &size, &count)
                    .IsSuccess());
    EXPECT_EQ(gfx::Size(kDefaultSize, kDefaultSize), size);
    EXPECT_EQ(kDefaultCount, count);
  }

  // Trading resolution for count within the memory budget is accepted
  // (e.g. 250x250 / 1800 = same total budget as 500x500 / 450).
  {
    gfx::Size size;
    int count = 0;
    EXPECT_TRUE(
        TracingHandler::ResolveScreenshotParams(250, 1800, &size, &count)
            .IsSuccess());
    EXPECT_EQ(gfx::Size(250, 250), size);
    EXPECT_EQ(1800, count);
  }

  // Non-positive values are rejected.
  {
    gfx::Size size;
    int count = 0;
    EXPECT_FALSE(
        TracingHandler::ResolveScreenshotParams(0, kDefaultCount, &size, &count)
            .IsSuccess());
    EXPECT_FALSE(
        TracingHandler::ResolveScreenshotParams(kDefaultSize, 0, &size, &count)
            .IsSuccess());
    EXPECT_FALSE(TracingHandler::ResolveScreenshotParams(-1, kDefaultCount,
                                                         &size, &count)
                     .IsSuccess());
  }

  // Combinations that exceed the per-session memory budget are rejected.
  {
    gfx::Size size;
    int count = 0;
    // 500x500 * 4 * 451 exceeds the 500x500 * 4 * 450 budget by one frame.
    EXPECT_FALSE(TracingHandler::ResolveScreenshotParams(
                     kDefaultSize, kDefaultCount + 1, &size, &count)
                     .IsSuccess());
  }
}

TEST_P(TracingHandlerTest, FilterUntrustedDataSources) {
  perfetto::TraceConfig config;
  config.add_data_sources()->mutable_config()->set_name("track_event");
  config.add_data_sources()->mutable_config()->set_name(
      tracing::kMetaData2SourceName);
  config.add_data_sources()->mutable_config()->set_name(
      "org.chromium.memory_instrumentation");
  config.add_data_sources()->mutable_config()->set_name(
      "org.chromium.sampler_profiler");
  config.add_data_sources()->mutable_config()->set_name("unknown_data_source");

  ASSERT_EQ(5, config.data_sources_size());

  TracingHandler::FilterUntrustedDataSources(config);

  ASSERT_EQ(2, config.data_sources_size());
  EXPECT_EQ("track_event", config.data_sources()[0].config().name());
  EXPECT_EQ(tracing::kMetaData2SourceName,
            config.data_sources()[1].config().name());
}

TEST_P(TracingHandlerTest, StartDropsClientFilters) {
  bool is_trusted = GetParam();

  perfetto::TraceConfig config;
  auto* data_source = config.add_data_sources();
  data_source->mutable_config()->set_name("track_event");
  data_source->add_producer_name_regex_filter(".*");
  data_source->add_producer_name_filter("old_filter");

  std::unordered_set<base::ProcessId> pids;
  if (is_trusted) {
    pids.insert(base::Process::Current().Pid());
  } else {
    pids.erase(base::Process::Current().Pid());
  }

  for (auto& ds : *config.mutable_data_sources()) {
    if (ds.config().name() == "track_event") {
      ds.clear_producer_name_regex_filter();
      ds.clear_producer_name_filter();
    }
  }

  CallAddPidsToProcessFilter(pids, config);

  ASSERT_EQ(1, config.data_sources_size());

  EXPECT_EQ(0, config.data_sources()[0].producer_name_regex_filter_size());

  if (is_trusted) {
    EXPECT_THAT(config.data_sources()[0].producer_name_filter(),
                testing::Not(testing::Contains("old_filter")));
    EXPECT_THAT(
        config.data_sources()[0].producer_name_filter(),
        testing::Contains(std::string(tracing::kPerfettoProducerNamePrefix) +
                          base::NumberToString(static_cast<uint32_t>(
                              base::Process::Current().Pid()))));
  } else {
    EXPECT_THAT(config.data_sources()[0].producer_name_filter(),
                testing::Not(testing::Contains("old_filter")));
    EXPECT_THAT(config.data_sources()[0].producer_name_filter(),
                testing::Not(testing::Contains(
                    std::string(tracing::kPerfettoProducerNamePrefix) +
                    base::NumberToString(static_cast<uint32_t>(
                        base::Process::Current().Pid())))));
    EXPECT_THAT(config.data_sources()[0].producer_name_filter(),
                testing::Contains("org.chromium-0"));
  }
}

INSTANTIATE_TEST_SUITE_P(All, TracingHandlerTest, testing::Bool());

}  // namespace protocol
}  // namespace content
