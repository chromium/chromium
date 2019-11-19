// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/trace_event/trace_config.h"
#include "base/values.h"
#include "content/browser/devtools/protocol/tracing_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace protocol {

namespace {

const char kCustomTraceConfigString[] =
    "{"
    "\"enable_argument_filter\":true,"
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
    "\"record_mode\":\"record-continuously\""
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
    "\"recordMode\":\"recordContinuously\""
    "}";

}  // namespace

class TracingHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    tracing_handler_.reset(new TracingHandler(nullptr, nullptr));
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
  std::unique_ptr<base::Value> value =
      base::JSONReader::ReadDeprecated(kCustomTraceConfigStringDevToolsStyle);
  std::unique_ptr<base::DictionaryValue> devtools_style_dict(
      static_cast<base::DictionaryValue*>(value.release()));

  base::trace_event::TraceConfig trace_config =
      TracingHandler::GetTraceConfigFromDevToolsConfig(*devtools_style_dict);

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

}  // namespace protocol
}  // namespace content
