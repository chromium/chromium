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
  TracingUITest() {}
};

std::string GetConfig() {
  base::Value dict(base::Value::Type::DICTIONARY);
  base::Value filter1(base::trace_event::MemoryDumpManager::kTraceCategory);
  base::Value filter2("filter2");
  base::Value included(base::Value::Type::LIST);
  included.Append(std::move(filter1));
  base::Value excluded(base::Value::Type::LIST);
  excluded.Append(std::move(filter2));

  dict.SetKey("included_categories", std::move(included));
  dict.SetKey("excluded_categories", std::move(excluded));
  dict.SetStringKey("record_mode", "record-continuously");
  dict.SetBoolKey("enable_systrace", true);
  dict.SetStringKey("stream_format", "protobuf");

  base::Value memory_config(base::Value::Type::DICTIONARY);
  base::Value trigger(base::Value::Type::DICTIONARY);
  trigger.SetStringKey("mode", "detailed");
  trigger.SetIntKey("periodic_interval_ms", 10000);
  base::Value triggers(base::Value::Type::LIST);
  triggers.Append(std::move(trigger));
  memory_config.SetKey("triggers", std::move(triggers));
  dict.SetKey("memory_dump_config", std::move(memory_config));

  std::string results;
  if (!base::JSONWriter::Write(dict, &results))
    return "";

  std::string data;
  base::Base64Encode(results, &data);
  return data;
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
            base::trace_event::MemoryDumpLevelOfDetail::DETAILED);
}

}  // namespace content
