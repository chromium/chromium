// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

class MockNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override { return type_; }
  void set_type(ConnectionType type) { type_ = type; }

 private:
  ConnectionType type_;
};

}  // namespace

class BackgroundTracingConfigTest : public testing::Test {
 public:
  BackgroundTracingConfigTest() = default;

 protected:
  BrowserTaskEnvironment task_environment_;
};

std::unique_ptr<BackgroundTracingConfigImpl> ReadFromJSONString(
    const std::string& json_text) {
  absl::optional<base::Value> json_value(base::JSONReader::Read(json_text));

  if (!json_value || !json_value->is_dict())
    return nullptr;

  std::unique_ptr<BackgroundTracingConfigImpl> config(
      static_cast<BackgroundTracingConfigImpl*>(
          BackgroundTracingConfig::FromDict(std::move(*json_value).TakeDict())
              .release()));
  return config;
}

std::string ConfigToString(BackgroundTracingConfig* config) {
  std::string results;
  if (base::JSONWriter::Write(config->ToDict(), &results))
    return results;
  return "";
}

std::string RuleToString(const std::unique_ptr<BackgroundTracingRule>& rule) {
  std::string results;
  if (base::JSONWriter::Write(rule->ToDict(), &results))
    return results;
  return "";
}

TEST_F(BackgroundTracingConfigTest, ConfigFromInvalidString) {
  // Missing or invalid mode
  EXPECT_FALSE(ReadFromJSONString("{}"));
  EXPECT_FALSE(ReadFromJSONString("{\"mode\":\"invalid\"}"));
}

TEST_F(BackgroundTracingConfigTest, PreemptiveConfigFromInvalidString) {
  // Missing or invalid category
  EXPECT_FALSE(ReadFromJSONString("{\"mode\":\"preemptive\"}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"invalid\"}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"invalid\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", \"trigger_name\":\"foo\"}]}"));

  // Missing rules.
  EXPECT_FALSE(
      ReadFromJSONString("{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
                         "\"BENCHMARK_STARTUP\",\"configs\": []}"));

  // Missing or invalid configs
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\"}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": \"\"}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": {}}"));

  // Invalid config entries
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": [{}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": [\"invalid\"]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": [[]]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": [{\"rule\": \"invalid\"}]}"));

  // Missing or invalid keys for a named trigger.
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": [{\"rule\": \"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\"}]}"));

  // Missing or invalid keys for a histogram trigger.
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\", "
      "\"histogram_name\":\"foo\"}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\", "
      "\"histogram_lower_value\": 1, \"histogram_upper_value\": 2}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\", "
      "\"histogram_name\":\"foo\", \"histogram_lower_value\": 1,"
      "\"histogram_upper_value\": 1}]}"));
}

TEST_F(BackgroundTracingConfigTest, ReactiveConfigFromInvalidString) {
  // Missing or invalid configs
  EXPECT_FALSE(ReadFromJSONString("{\"mode\":\"reactive\"}"));
  EXPECT_FALSE(
      ReadFromJSONString("{\"mode\":\"reactive\", \"configs\": \"invalid\"}"));
  EXPECT_FALSE(ReadFromJSONString("{\"mode\":\"reactive\", \"configs\": {}}"));

  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\", \"configs\": []}"));

  // Invalid config entries
  EXPECT_FALSE(
      ReadFromJSONString("{\"mode\":\"reactive\", \"configs\": [{}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\", \"configs\": [\"invalid\"]}"));

  // Invalid tracing rule type
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": []}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": \"\"}]}"));
  EXPECT_FALSE(
      ReadFromJSONString("{\"mode\":\"reactive\","
                         "\"configs\": [{\"rule\": "
                         "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\"}]}"));

  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": "
      "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\", \"category\": "
      "[]}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": "
      "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\", \"category\": "
      "\"\"}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": "
      "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\", \"category\": "
      "\"benchmark\"}]}"));

  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": "
      "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\", \"category\": "
      "\"benchmark\", \"trigger_name\": []}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": "
      "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\", \"category\": "
      "\"benchmark\", \"trigger_name\": 0}]}"));
}

TEST_F(BackgroundTracingConfigTest, PreemptiveConfigFromValidString) {
  std::unique_ptr<BackgroundTracingConfigImpl> config;

  config = ReadFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK_STARTUP\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", \"trigger_name\":\"foo\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::PREEMPTIVE);
  EXPECT_EQ(config->category_preset(),
            BackgroundTracingConfigImpl::BENCHMARK_STARTUP);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"rule\":\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\","
            "\"trigger_name\":\"foo\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK_STARTUP\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\", "
      "\"histogram_name\":\"foo\", \"histogram_value\": 1}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::PREEMPTIVE);
  EXPECT_EQ(config->category_preset(),
            BackgroundTracingConfigImpl::BENCHMARK_STARTUP);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"histogram_lower_value\":1,\"histogram_name\":\"foo\","
            "\"histogram_repeat\":true,\"histogram_upper_value\":2147483647,"
            "\"rule\":\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK_STARTUP\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\", "
      "\"histogram_name\":\"foo\", \"histogram_value\": 1, "
      "\"histogram_repeat\":false}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::PREEMPTIVE);
  EXPECT_EQ(config->category_preset(),
            BackgroundTracingConfigImpl::BENCHMARK_STARTUP);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"histogram_lower_value\":1,\"histogram_name\":\"foo\","
            "\"histogram_repeat\":false,\"histogram_upper_value\":2147483647,"
            "\"rule\":\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK_STARTUP\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\", "
      "\"histogram_name\":\"foo\", \"histogram_lower_value\": 1, "
      "\"histogram_upper_value\": 2}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::PREEMPTIVE);
  EXPECT_EQ(config->category_preset(),
            BackgroundTracingConfigImpl::BENCHMARK_STARTUP);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"histogram_lower_value\":1,\"histogram_name\":\"foo\","
            "\"histogram_repeat\":true,\"histogram_upper_value\":2,\"rule\":"
            "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK_STARTUP\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\", "
      "\"histogram_name\":\"foo\", \"histogram_lower_value\": 1, "
      "\"histogram_upper_value\": 2, \"histogram_repeat\":false}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::PREEMPTIVE);
  EXPECT_EQ(config->category_preset(),
            BackgroundTracingConfigImpl::BENCHMARK_STARTUP);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"histogram_lower_value\":1,\"histogram_name\":\"foo\","
            "\"histogram_repeat\":false,\"histogram_upper_value\":2,\"rule\":"
            "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK_STARTUP\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\", "
      "\"histogram_name\":\"foo\", \"histogram_value\": 1}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::PREEMPTIVE);
  EXPECT_EQ(config->category_preset(),
            BackgroundTracingConfigImpl::BENCHMARK_STARTUP);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"histogram_lower_value\":1,\"histogram_name\":\"foo\","
            "\"histogram_repeat\":true,\"histogram_upper_value\":2147483647,"
            "\"rule\":\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK_STARTUP\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", \"trigger_name\":\"foo1\"}, "
      "{\"rule\": \"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", "
      "\"trigger_name\":\"foo2\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::PREEMPTIVE);
  EXPECT_EQ(config->category_preset(),
            BackgroundTracingConfigImpl::BENCHMARK_STARTUP);
  EXPECT_EQ(config->rules().size(), 2u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"rule\":\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\","
            "\"trigger_name\":\"foo1\"}");
  EXPECT_EQ(RuleToString(config->rules()[1]),
            "{\"rule\":\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\","
            "\"trigger_name\":\"foo2\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"custom_categories\": "
      "\"toplevel,benchmark\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", \"trigger_name\":\"foo1\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::PREEMPTIVE);
  EXPECT_EQ(config->category_preset(),
            BackgroundTracingConfigImpl::CUSTOM_CATEGORY_PRESET);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(
      ConfigToString(config.get()),
      "{\"category\":\"CUSTOM\",\"configs\":[{\"rule\":\"MONITOR_AND_DUMP_WHEN_"
      "TRIGGER_NAMED\",\"trigger_name\":\"foo1\"}],\"custom_categories\":"
      "\"toplevel,benchmark\",\"mode\":\"PREEMPTIVE_TRACING_MODE\"}");
}

TEST_F(BackgroundTracingConfigTest, ValidPreemptiveCategoryToString) {
  std::unique_ptr<BackgroundTracingConfigImpl> config = ReadFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK_STARTUP\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", \"trigger_name\":\"foo\"}]}");

  constexpr BackgroundTracingConfigImpl::CategoryPreset kCategoryPreset =
      BackgroundTracingConfigImpl::BENCHMARK_STARTUP;
  constexpr const char kCategoryString[] = "BENCHMARK_STARTUP";

  config->set_category_preset(kCategoryPreset);
  std::string expected =
      std::string("{\"category\":\"") + kCategoryString +
      std::string(
          "\",\"configs\":[{\"rule\":"
          "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\",\"trigger_name\":"
          "\"foo\"}],\"mode\":\"PREEMPTIVE_TRACING_MODE\"}");
  EXPECT_EQ(ConfigToString(config.get()), expected.c_str());
  std::unique_ptr<BackgroundTracingConfigImpl> config2 =
      ReadFromJSONString(expected);
  EXPECT_EQ(config->category_preset(), config2->category_preset());
}

TEST_F(BackgroundTracingConfigTest, ReactiveConfigFromValidString) {
  std::unique_ptr<BackgroundTracingConfigImpl> config;

  config = ReadFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\",\"configs\": [{\"rule\": "
      "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\", "
      "\"category\": \"BENCHMARK_STARTUP\",\"trigger_delay\":30,"
      "\"trigger_name\": \"foo\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::REACTIVE);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"category\":\"BENCHMARK_STARTUP\","
            "\"rule\":\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\","
            "\"trigger_delay\":30,\"trigger_name\":\"foo\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\",\"configs\": [{\"rule\": "
      "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\", "
      "\"category\": \"BENCHMARK_STARTUP\", \"trigger_delay\":30, "
      "\"trigger_name\": \"foo\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::REACTIVE);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"category\":\"BENCHMARK_STARTUP\","
            "\"rule\":\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\","
            "\"trigger_delay\":30,\"trigger_name\":\"foo\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\",\"configs\": [{\"rule\": "
      "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\", "
      "\"category\": \"BENCHMARK_STARTUP\",\"trigger_delay\":30,"
      "\"trigger_name\": \"foo\",\"trigger_delay\":30,"
      "\"trigger_chance\": 0.5}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::REACTIVE);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"category\":\"BENCHMARK_STARTUP\","
            "\"rule\":\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\","
            "\"trigger_chance\":0.5,\"trigger_delay\":30,"
            "\"trigger_name\":\"foo\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\",\"configs\": [{\"rule\": "
      "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\", "
      "\"category\": \"BENCHMARK_STARTUP\", \"trigger_name\": "
      "\"foo1\"},{\"rule\": "
      "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\", "
      "\"category\": \"BENCHMARK_STARTUP\", \"trigger_name\": \"foo2\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::REACTIVE);
  EXPECT_EQ(config->rules().size(), 2u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"category\":\"BENCHMARK_STARTUP\","
            "\"rule\":\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\","
            "\"trigger_delay\":30,\"trigger_name\":\"foo1\"}");
  EXPECT_EQ(RuleToString(config->rules()[1]),
            "{\"category\":\"BENCHMARK_STARTUP\","
            "\"rule\":\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\","
            "\"trigger_delay\":30,\"trigger_name\":\"foo2\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\",\"configs\": [{\"rule\": "
      "\"TRACE_AT_RANDOM_INTERVALS\","
      "\"category\": \"BENCHMARK_STARTUP\","
      "\"timeout_min\":10, \"timeout_max\":20}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::REACTIVE);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"category\":\"BENCHMARK_STARTUP\",\"rule\":\"TRACE_AT_RANDOM_"
            "INTERVALS\","
            "\"timeout_max\":20,\"timeout_min\":10}");

  config = ReadFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\","
      "\"custom_categories\": \"benchmark,toplevel\","
      "\"configs\": [{\"rule\": "
      "\"TRACE_AT_RANDOM_INTERVALS\","
      "\"timeout_max\":20,\"timeout_min\":10}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::REACTIVE);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(ConfigToString(config.get()),
            "{\"configs\":[{\"category\":\"CUSTOM\",\"rule\":\"TRACE_AT_RANDOM_"
            "INTERVALS\",\"timeout_"
            "max\":20,\"timeout_min\":10}],\"custom_categories\":\"benchmark,"
            "toplevel\",\"mode\":\"REACTIVE_TRACING_MODE\"}");
}

TEST_F(BackgroundTracingConfigTest, ValidPreemptiveConfigToString) {
  std::unique_ptr<BackgroundTracingConfigImpl> config(
      new BackgroundTracingConfigImpl(BackgroundTracingConfig::PREEMPTIVE));

  // Default values
  EXPECT_EQ(ConfigToString(config.get()),
            "{\"category\":\"BENCHMARK_STARTUP\",\"configs\":[],\"mode\":"
            "\"PREEMPTIVE_"
            "TRACING_MODE\"}");

  // Change category_preset
  config->set_category_preset(BackgroundTracingConfigImpl::BENCHMARK_STARTUP);
  EXPECT_EQ(ConfigToString(config.get()),
            "{\"category\":\"BENCHMARK_STARTUP\",\"configs\":[],\"mode\":"
            "\"PREEMPTIVE_TRACING_MODE\"}");

  {
    config = std::make_unique<BackgroundTracingConfigImpl>(
        BackgroundTracingConfig::PREEMPTIVE);
    config->set_category_preset(BackgroundTracingConfigImpl::BENCHMARK_STARTUP);

    base::Value::Dict dict;
    dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    dict.Set("trigger_name", "foo");
    config->AddPreemptiveRule(dict);

    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK_STARTUP\",\"configs\":[{\"rule\":"
              "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\",\"trigger_name\":"
              "\"foo\"}],\"mode\":\"PREEMPTIVE_TRACING_MODE\"}");
  }

  {
    config = std::make_unique<BackgroundTracingConfigImpl>(
        BackgroundTracingConfig::PREEMPTIVE);
    config->set_category_preset(BackgroundTracingConfigImpl::BENCHMARK_STARTUP);

    base::Value::Dict dict;
    dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    dict.Set("trigger_name", "foo");
    dict.Set("trigger_chance", 0.5);
    config->AddPreemptiveRule(dict);

    EXPECT_EQ(
        ConfigToString(config.get()),
        "{\"category\":\"BENCHMARK_STARTUP\",\"configs\":[{\"rule\":"
        "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\",\"trigger_chance\":0.5,"
        "\"trigger_name\":\"foo\"}],\"mode\":\"PREEMPTIVE_TRACING_MODE\"}");
  }

  {
    config = std::make_unique<BackgroundTracingConfigImpl>(
        BackgroundTracingConfig::PREEMPTIVE);
    config->set_category_preset(BackgroundTracingConfigImpl::BENCHMARK_STARTUP);

    base::Value::Dict dict;
    dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    dict.Set("trigger_name", "foo1");
    config->AddPreemptiveRule(dict);

    dict.Set("trigger_name", "foo2");
    config->AddPreemptiveRule(dict);

    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK_STARTUP\",\"configs\":[{\"rule\":"
              "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\",\"trigger_name\":"
              "\"foo1\"},{\"rule\":\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\","
              "\"trigger_name\":\"foo2\"}],\"mode\":\"PREEMPTIVE_TRACING_"
              "MODE\"}");
  }

  {
    config = std::make_unique<BackgroundTracingConfigImpl>(
        BackgroundTracingConfig::PREEMPTIVE);

    base::Value::Dict second_dict;
    second_dict.Set("rule",
                    "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    second_dict.Set("histogram_name", "foo");
    second_dict.Set("histogram_lower_value", 1);
    second_dict.Set("histogram_upper_value", 2);
    config->AddPreemptiveRule(second_dict);

    EXPECT_EQ(
        ConfigToString(config.get()),
        "{\"category\":\"BENCHMARK_STARTUP\",\"configs\":[{\"histogram_lower_"
        "value\":1,\"histogram_name\":\"foo\",\"histogram_repeat\":true,"
        "\"histogram_upper_value\":2,\"rule\":\"MONITOR_AND_DUMP_WHEN_"
        "SPECIFIC_HISTOGRAM_AND_VALUE\"}],\"mode\":\"PREEMPTIVE_TRACING_"
        "MODE\"}");
  }

  {
    config = std::make_unique<BackgroundTracingConfigImpl>(
        BackgroundTracingConfig::PREEMPTIVE);

    base::Value::Dict second_dict;
    second_dict.Set("rule",
                    "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    second_dict.Set("histogram_name", "foo");
    second_dict.Set("histogram_lower_value", 1);
    second_dict.Set("histogram_upper_value", 2);
    second_dict.Set("trigger_delay", 10);
    config->AddPreemptiveRule(second_dict);

    EXPECT_EQ(
        ConfigToString(config.get()),
        "{\"category\":\"BENCHMARK_STARTUP\",\"configs\":[{\"histogram_lower_"
        "value\":1,\"histogram_name\":\"foo\",\"histogram_repeat\":true,"
        "\"histogram_upper_value\":2,\"rule\":\"MONITOR_AND_DUMP_WHEN_"
        "SPECIFIC_HISTOGRAM_AND_VALUE\",\"trigger_delay\":10}],\"mode\":"
        "\"PREEMPTIVE_TRACING_MODE\"}");
  }
}

TEST_F(BackgroundTracingConfigTest, InvalidPreemptiveConfigToString) {
  std::unique_ptr<BackgroundTracingConfigImpl> config;

  {
    config = std::make_unique<BackgroundTracingConfigImpl>(
        BackgroundTracingConfig::PREEMPTIVE);

    base::Value::Dict dict;
    dict.Set("rule", "MONITOR_AND_DUMP_WHEN_BROWSER_STARTUP_COMPLETE");
    config->AddPreemptiveRule(dict);

    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK_STARTUP\",\"configs\":[],\"mode\":"
              "\"PREEMPTIVE_TRACING_MODE\"}");
  }

  {
    config = std::make_unique<BackgroundTracingConfigImpl>(
        BackgroundTracingConfig::PREEMPTIVE);

    // TODO(crbug.com/1247459): |second_dict| is not used.
    base::Value::Dict second_dict;
    second_dict.Set("rule",
                    "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    second_dict.Set("histogram_name", "foo");
    second_dict.Set("histogram_lower_value", 1);

    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK_STARTUP\",\"configs\":[],\"mode\":"
              "\"PREEMPTIVE_TRACING_MODE\"}");
  }

  {
    config = std::make_unique<BackgroundTracingConfigImpl>(
        BackgroundTracingConfig::PREEMPTIVE);

    // TODO(crbug.com/1247459): |second_dict| is not used.
    base::Value::Dict second_dict;
    second_dict.Set("rule",
                    "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    second_dict.Set("histogram_name", "foo");
    second_dict.Set("histogram_lower_value", 1);
    second_dict.Set("histogram_upper_value", 1);

    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK_STARTUP\",\"configs\":[],\"mode\":"
              "\"PREEMPTIVE_TRACING_MODE\"}");
  }
}

TEST_F(BackgroundTracingConfigTest, ValidReactiveConfigToString) {
  std::unique_ptr<BackgroundTracingConfigImpl> config(
      new BackgroundTracingConfigImpl(BackgroundTracingConfig::REACTIVE));

  // Default values
  EXPECT_EQ(ConfigToString(config.get()),
            "{\"configs\":[],\"mode\":\"REACTIVE_TRACING_MODE\"}");

  {
    config = std::make_unique<BackgroundTracingConfigImpl>(
        BackgroundTracingConfig::REACTIVE);

    base::Value::Dict dict;
    dict.Set("rule", "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
    dict.Set("trigger_name", "foo");
    config->AddReactiveRule(dict,
                            BackgroundTracingConfigImpl::BENCHMARK_STARTUP);

    EXPECT_EQ(
        ConfigToString(config.get()),
        "{\"configs\":[{\"category\":\"BENCHMARK_STARTUP\",\"rule\":\"TRACE_"
        "ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\",\"trigger_delay\":30,"
        "\"trigger_name\":\"foo\"}],\"mode\":\"REACTIVE_TRACING_MODE\"}");
  }

  {
    config = std::make_unique<BackgroundTracingConfigImpl>(
        BackgroundTracingConfig::REACTIVE);

    base::Value::Dict dict;
    dict.Set("rule", "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
    dict.Set("trigger_name", "foo1");
    config->AddReactiveRule(dict,
                            BackgroundTracingConfigImpl::BENCHMARK_STARTUP);

    dict.Set("trigger_name", "foo2");
    config->AddReactiveRule(dict,
                            BackgroundTracingConfigImpl::BENCHMARK_STARTUP);

    EXPECT_EQ(
        ConfigToString(config.get()),
        "{\"configs\":[{\"category\":\"BENCHMARK_STARTUP\",\"rule\":\"TRACE_"
        "ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\",\"trigger_delay\":30,"
        "\"trigger_name\":\"foo1\"},{\"category\":\"BENCHMARK_STARTUP\","
        "\"rule\":"
        "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\",\"trigger_delay\":30,"
        "\"trigger_name\":\"foo2\"}],\"mode\":\"REACTIVE_TRACING_MODE\"}");
  }
}

TEST_F(BackgroundTracingConfigTest, BufferLimitConfig) {
  MockNetworkChangeNotifier notifier;

  std::unique_ptr<BackgroundTracingConfigImpl> config;

  config = ReadFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\",\"configs\": [{\"rule\": "
      "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\", "
      "\"category\": \"BENCHMARK_STARTUP\",\"trigger_delay\":30,"
      "\"trigger_name\": \"foo\"}],\"low_ram_buffer_size_kb\":800,"
      "\"medium_ram_buffer_size_kb\":1000,\"mobile_network_buffer_size_kb\":"
      "300,\"max_buffer_size_kb\":1000,\"upload_limit_kb\":500,"
      "\"upload_limit_network_kb\":600}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::REACTIVE);
  EXPECT_EQ(config->rules().size(), 1u);

  notifier.set_type(net::NetworkChangeNotifier::CONNECTION_2G);
#if BUILDFLAG(IS_ANDROID)
  int64_t ram_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  size_t expected_trace_buffer_size =
      (ram_mb > 0 && ram_mb <= 1024) ? 800u : 300u;
  EXPECT_EQ(expected_trace_buffer_size,
            config->GetTraceConfig().GetTraceBufferSizeInKb());
  EXPECT_EQ(600u, config->GetTraceUploadLimitKb());
#endif

  notifier.set_type(net::NetworkChangeNotifier::CONNECTION_WIFI);
  EXPECT_LE(800u, config->GetTraceConfig().GetTraceBufferSizeInKb());
  EXPECT_EQ(500u, config->GetTraceUploadLimitKb());
}

}  // namespace content
