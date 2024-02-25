// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_config.h"
#include "base/values.h"
#include "content/browser/tracing/tracing_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class TracingUITest : public testing::Test {
 public:
  TracingUITest() = default;
};

std::string GetConfig() {
  auto dict =
      base::Value::Dict()
          .Set("included_categories",
               base::Value::List().Append(base::Value(
                   base::trace_event::MemoryDumpManager::kTraceCategory)))
          .Set("excluded_categories",
               base::Value::List().Append(base::Value("filter2")))
          .Set("record_mode", "record-continuously")
          .Set("enable_systrace", true)
          .Set("stream_format", "protobuf")
          .Set("memory_dump_config",
               base::Value::Dict().Set(
                   "triggers", base::Value::List().Append(
                                   base::Value::Dict()
                                       .Set("mode", "detailed")
                                       .Set("periodic_interval_ms", 10000))));

  std::string results;
  if (!base::JSONWriter::Write(dict, &results))
    return "";
  return base::Base64Encode(results);
}

TEST_F(TracingUITest, ConfigParsing) {
  base::trace_event::TraceConfig config;
  std::string stream_format;
  ASSERT_TRUE(TracingUI::GetTracingOptions(GetConfig(), config, stream_format));
  EXPECT_EQ(config.GetTraceRecordMode(),
            base::trace_event::RECORD_CONTINUOUSLY);
  std::string expected(base::trace_event::MemoryDumpManager::kTraceCategory);
  expected += ",-filter2";
  EXPECT_EQ(config.ToCategoryFilterString(), expected);
  EXPECT_EQ(stream_format, "protobuf");
  EXPECT_TRUE(config.IsSystraceEnabled());

  ASSERT_EQ(config.memory_dump_config().triggers.size(), 1u);
  EXPECT_EQ(config.memory_dump_config().triggers[0].min_time_between_dumps_ms,
            10000u);
  EXPECT_EQ(config.memory_dump_config().triggers[0].level_of_detail,
            base::trace_event::MemoryDumpLevelOfDetail::kDetailed);
}

}  // namespace content
