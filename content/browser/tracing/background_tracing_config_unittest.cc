// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  std::unique_ptr<base::Value> json_value(
      base::JSONReader::ReadDeprecated(json_text));

  base::DictionaryValue* dict = nullptr;
  if (json_value)
    json_value->GetAsDictionary(&dict);

  std::unique_ptr<BackgroundTracingConfigImpl> config(
      static_cast<BackgroundTracingConfigImpl*>(
          BackgroundTracingConfig::FromDict(dict).release()));
  return config;
}

std::string ConfigToString(BackgroundTracingConfig* config) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  config->IntoDict(dict.get());

  std::string results;
  if (base::JSONWriter::Write(*dict.get(), &results))
    return results;
  return "";
}

std::string RuleToString(const std::unique_ptr<BackgroundTracingRule>& rule) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  rule->IntoDict(dict.get());

  std::string results;
  if (base::JSONWriter::Write(*dict.get(), &results))
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
                         "\"BENCHMARK\",\"configs\": []}"));

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
      "\"BENCHMARK\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", \"trigger_name\":\"foo\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::PREEMPTIVE);
  EXPECT_EQ(config->category_preset(), BackgroundTracingConfigImpl::BENCHMARK);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"rule\":\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\","
            "\"trigger_name\":\"foo\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\", "
      "\"histogram_name\":\"foo\", \"histogram_value\": 1}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::PREEMPTIVE);
  EXPECT_EQ(config->category_preset(), BackgroundTracingConfigImpl::BENCHMARK);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"histogram_lower_value\":1,\"histogram_name\":\"foo\","
            "\"histogram_repeat\":true,\"histogram_upper_value\":2147483647,"
            "\"rule\":\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\", "
      "\"histogram_name\":\"foo\", \"histogram_value\": 1, "
      "\"histogram_repeat\":false}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::PREEMPTIVE);
  EXPECT_EQ(config->category_preset(), BackgroundTracingConfigImpl::BENCHMARK);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"histogram_lower_value\":1,\"histogram_name\":\"foo\","
            "\"histogram_repeat\":false,\"histogram_upper_value\":2147483647,"
            "\"rule\":\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\", "
      "\"histogram_name\":\"foo\", \"histogram_lower_value\": 1, "
      "\"histogram_upper_value\": 2}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::PREEMPTIVE);
  EXPECT_EQ(config->category_preset(), BackgroundTracingConfigImpl::BENCHMARK);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"histogram_lower_value\":1,\"histogram_name\":\"foo\","
            "\"histogram_repeat\":true,\"histogram_upper_value\":2,\"rule\":"
            "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\", "
      "\"histogram_name\":\"foo\", \"histogram_lower_value\": 1, "
      "\"histogram_upper_value\": 2, \"histogram_repeat\":false}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::PREEMPTIVE);
  EXPECT_EQ(config->category_preset(), BackgroundTracingConfigImpl::BENCHMARK);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"histogram_lower_value\":1,\"histogram_name\":\"foo\","
            "\"histogram_repeat\":false,\"histogram_upper_value\":2,\"rule\":"
            "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", \"trigger_name\":\"foo1\"}, "
      "{\"rule\": \"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", "
      "\"trigger_name\":\"foo2\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::PREEMPTIVE);
  EXPECT_EQ(config->category_preset(), BackgroundTracingConfigImpl::BENCHMARK);
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
      "\"BENCHMARK\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", \"trigger_name\":\"foo\"}]}");

  BackgroundTracingConfigImpl::CategoryPreset categories[] = {
      BackgroundTracingConfigImpl::BENCHMARK,
      BackgroundTracingConfigImpl::BENCHMARK_DEEP,
      BackgroundTracingConfigImpl::BENCHMARK_GPU,
      BackgroundTracingConfigImpl::BENCHMARK_IPC,
      BackgroundTracingConfigImpl::BENCHMARK_STARTUP,
      BackgroundTracingConfigImpl::BENCHMARK_BLINK_GC,
      BackgroundTracingConfigImpl::BENCHMARK_MEMORY_HEAVY,
      BackgroundTracingConfigImpl::BENCHMARK_MEMORY_LIGHT,
      BackgroundTracingConfigImpl::BENCHMARK_EXECUTION_METRIC,
      BackgroundTracingConfigImpl::BENCHMARK_NAVIGATION,
      BackgroundTracingConfigImpl::BENCHMARK_RENDERERS,
      BackgroundTracingConfigImpl::BENCHMARK_SERVICEWORKER,
      BackgroundTracingConfigImpl::BENCHMARK_POWER,
      BackgroundTracingConfigImpl::BLINK_STYLE,
  };

  const char* category_strings[] = {"BENCHMARK",
                                    "BENCHMARK_DEEP",
                                    "BENCHMARK_GPU",
                                    "BENCHMARK_IPC",
                                    "BENCHMARK_STARTUP",
                                    "BENCHMARK_BLINK_GC",
                                    "BENCHMARK_MEMORY_HEAVY",
                                    "BENCHMARK_MEMORY_LIGHT",
                                    "BENCHMARK_EXECUTION_METRIC",
                                    "BENCHMARK_NAVIGATION",
                                    "BENCHMARK_RENDERERS",
                                    "BENCHMARK_SERVICEWORKER",
                                    "BENCHMARK_POWER",
                                    "BLINK_STYLE"};
  for (size_t i = 0;
       i <
       sizeof(categories) / sizeof(BackgroundTracingConfigImpl::CategoryPreset);
       i++) {
    config->set_category_preset(categories[i]);
    std::string expected =
        std::string("{\"category\":\"") + category_strings[i] +
        std::string(
            "\",\"configs\":[{\"rule\":"
            "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\",\"trigger_name\":"
            "\"foo\"}],\"mode\":\"PREEMPTIVE_TRACING_MODE\"}");
    EXPECT_EQ(ConfigToString(config.get()), expected.c_str());
    std::unique_ptr<BackgroundTracingConfigImpl> config2 =
        ReadFromJSONString(expected);
    EXPECT_EQ(config->category_preset(), config2->category_preset());
  }
}

TEST_F(BackgroundTracingConfigTest, ReactiveConfigFromValidString) {
  std::unique_ptr<BackgroundTracingConfigImpl> config;

  config = ReadFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\",\"configs\": [{\"rule\": "
      "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\", "
      "\"category\": \"BENCHMARK\",\"trigger_delay\":30,"
      "\"trigger_name\": \"foo\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::REACTIVE);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"category\":\"BENCHMARK\","
            "\"rule\":\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\","
            "\"trigger_delay\":30,\"trigger_name\":\"foo\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\",\"configs\": [{\"rule\": "
      "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\", "
      "\"category\": \"BENCHMARK_DEEP\", \"trigger_delay\":30, "
      "\"trigger_name\": \"foo\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::REACTIVE);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"category\":\"BENCHMARK_DEEP\","
            "\"rule\":\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\","
            "\"trigger_delay\":30,\"trigger_name\":\"foo\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\",\"configs\": [{\"rule\": "
      "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\", "
      "\"category\": \"BENCHMARK_DEEP\",\"trigger_delay\":30,"
      "\"trigger_name\": \"foo\",\"trigger_delay\":30,"
      "\"trigger_chance\": 0.5}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::REACTIVE);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"category\":\"BENCHMARK_DEEP\","
            "\"rule\":\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\","
            "\"trigger_chance\":0.5,\"trigger_delay\":30,"
            "\"trigger_name\":\"foo\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\",\"configs\": [{\"rule\": "
      "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\", "
      "\"category\": \"BENCHMARK_DEEP\", \"trigger_name\": "
      "\"foo1\"},{\"rule\": "
      "\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\", "
      "\"category\": \"BENCHMARK_DEEP\", \"trigger_name\": \"foo2\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::REACTIVE);
  EXPECT_EQ(config->rules().size(), 2u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"category\":\"BENCHMARK_DEEP\","
            "\"rule\":\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\","
            "\"trigger_delay\":30,\"trigger_name\":\"foo1\"}");
  EXPECT_EQ(RuleToString(config->rules()[1]),
            "{\"category\":\"BENCHMARK_DEEP\","
            "\"rule\":\"TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\","
            "\"trigger_delay\":30,\"trigger_name\":\"foo2\"}");

  config = ReadFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\",\"configs\": [{\"rule\": "
      "\"TRACE_AT_RANDOM_INTERVALS\","
      "\"stop_tracing_on_repeated_reactive\": true,"
      "\"category\": \"BENCHMARK_DEEP\","
      "\"timeout_min\":10, \"timeout_max\":20}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::REACTIVE);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"category\":\"BENCHMARK_DEEP\",\"rule\":\"TRACE_AT_RANDOM_"
            "INTERVALS\",\"stop_tracing_on_repeated_reactive\":true,"
            "\"timeout_max\":20,\"timeout_min\":10}");

  config = ReadFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\","
      "\"custom_categories\": \"benchmark,toplevel\","
      "\"configs\": [{\"rule\": "
      "\"TRACE_AT_RANDOM_INTERVALS\","
      "\"stop_tracing_on_repeated_reactive\": true, "
      "\"timeout_max\":20,\"timeout_min\":10}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::REACTIVE);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(ConfigToString(config.get()),
            "{\"configs\":[{\"category\":\"CUSTOM\",\"rule\":\"TRACE_AT_RANDOM_"
            "INTERVALS\",\"stop_tracing_on_repeated_reactive\":true,\"timeout_"
            "max\":20,\"timeout_min\":10}],\"custom_categories\":\"benchmark,"
            "toplevel\",\"mode\":\"REACTIVE_TRACING_MODE\"}");
}

TEST_F(BackgroundTracingConfigTest, ValidPreemptiveConfigToString) {
  std::unique_ptr<BackgroundTracingConfigImpl> config(
      new BackgroundTracingConfigImpl(BackgroundTracingConfig::PREEMPTIVE));

  // Default values
  EXPECT_EQ(ConfigToString(config.get()),
            "{\"category\":\"BENCHMARK\",\"configs\":[],\"mode\":\"PREEMPTIVE_"
            "TRACING_MODE\"}");

  // Change category_preset
  config->set_category_preset(BackgroundTracingConfigImpl::BENCHMARK_DEEP);
  EXPECT_EQ(ConfigToString(config.get()),
            "{\"category\":\"BENCHMARK_DEEP\",\"configs\":[],\"mode\":"
            "\"PREEMPTIVE_TRACING_MODE\"}");

  {
    config.reset(
        new BackgroundTracingConfigImpl(BackgroundTracingConfig::PREEMPTIVE));
    config->set_category_preset(BackgroundTracingConfigImpl::BENCHMARK_DEEP);

    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    dict->SetString("trigger_name", "foo");
    config->AddPreemptiveRule(dict.get());

    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK_DEEP\",\"configs\":[{\"rule\":"
              "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\",\"trigger_name\":"
              "\"foo\"}],\"mode\":\"PREEMPTIVE_TRACING_MODE\"}");
  }

  {
    config.reset(
        new BackgroundTracingConfigImpl(BackgroundTracingConfig::PREEMPTIVE));
    config->set_category_preset(BackgroundTracingConfigImpl::BENCHMARK_DEEP);

    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    dict->SetString("trigger_name", "foo");
    dict->SetDouble("trigger_chance", 0.5);
    config->AddPreemptiveRule(dict.get());

    EXPECT_EQ(
        ConfigToString(config.get()),
        "{\"category\":\"BENCHMARK_DEEP\",\"configs\":[{\"rule\":"
        "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\",\"trigger_chance\":0.5,"
        "\"trigger_name\":\"foo\"}],\"mode\":\"PREEMPTIVE_TRACING_MODE\"}");
  }

  {
    config.reset(
        new BackgroundTracingConfigImpl(BackgroundTracingConfig::PREEMPTIVE));
    config->set_category_preset(BackgroundTracingConfigImpl::BENCHMARK_DEEP);

    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    dict->SetString("trigger_name", "foo1");
    config->AddPreemptiveRule(dict.get());

    dict->SetString("trigger_name", "foo2");
    config->AddPreemptiveRule(dict.get());

    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK_DEEP\",\"configs\":[{\"rule\":"
              "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\",\"trigger_name\":"
              "\"foo1\"},{\"rule\":\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\","
              "\"trigger_name\":\"foo2\"}],\"mode\":\"PREEMPTIVE_TRACING_"
              "MODE\"}");
  }

  {
    config.reset(
        new BackgroundTracingConfigImpl(BackgroundTracingConfig::PREEMPTIVE));

    std::unique_ptr<base::DictionaryValue> second_dict(
        new base::DictionaryValue());
    second_dict->SetString(
        "rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    second_dict->SetString("histogram_name", "foo");
    second_dict->SetInteger("histogram_lower_value", 1);
    second_dict->SetInteger("histogram_upper_value", 2);
    config->AddPreemptiveRule(second_dict.get());

    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK\",\"configs\":[{\"histogram_lower_"
              "value\":1,\"histogram_name\":\"foo\",\"histogram_repeat\":true,"
              "\"histogram_upper_value\":2,\"rule\":\"MONITOR_AND_DUMP_WHEN_"
              "SPECIFIC_HISTOGRAM_AND_VALUE\"}],\"mode\":\"PREEMPTIVE_TRACING_"
              "MODE\"}");
  }

  {
    config.reset(
        new BackgroundTracingConfigImpl(BackgroundTracingConfig::PREEMPTIVE));

    std::unique_ptr<base::DictionaryValue> second_dict(
        new base::DictionaryValue());
    second_dict->SetString(
        "rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    second_dict->SetString("histogram_name", "foo");
    second_dict->SetInteger("histogram_lower_value", 1);
    second_dict->SetInteger("histogram_upper_value", 2);
    second_dict->SetInteger("trigger_delay", 10);
    config->AddPreemptiveRule(second_dict.get());

    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK\",\"configs\":[{\"histogram_lower_"
              "value\":1,\"histogram_name\":\"foo\",\"histogram_repeat\":true,"
              "\"histogram_upper_value\":2,\"rule\":\"MONITOR_AND_DUMP_WHEN_"
              "SPECIFIC_HISTOGRAM_AND_VALUE\",\"trigger_delay\":10}],\"mode\":"
              "\"PREEMPTIVE_TRACING_MODE\"}");
  }

  {
    config.reset(
        new BackgroundTracingConfigImpl(BackgroundTracingConfig::PREEMPTIVE));

    std::unique_ptr<base::DictionaryValue> second_dict(
        new base::DictionaryValue());
    second_dict->SetString(
        "rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    second_dict->SetString("histogram_name", "foo");
    second_dict->SetInteger("histogram_lower_value", 1);
    second_dict->SetInteger("histogram_upper_value", 2);
    second_dict->SetInteger("trigger_delay", 10);
    config->AddPreemptiveRule(second_dict.get());

    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK\",\"configs\":[{\"histogram_lower_"
              "value\":1,\"histogram_name\":\"foo\",\"histogram_repeat\":true,"
              "\"histogram_upper_value\":2,\"rule\":\"MONITOR_AND_DUMP_WHEN_"
              "SPECIFIC_HISTOGRAM_AND_VALUE\",\"trigger_delay\":10}],\"mode\":"
              "\"PREEMPTIVE_TRACING_MODE\"}");
  }
}

TEST_F(BackgroundTracingConfigTest, InvalidPreemptiveConfigToString) {
  std::unique_ptr<BackgroundTracingConfigImpl> config;

  {
    config.reset(
        new BackgroundTracingConfigImpl(BackgroundTracingConfig::PREEMPTIVE));

    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_BROWSER_STARTUP_COMPLETE");
    config->AddPreemptiveRule(dict.get());

    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK\",\"configs\":[],\"mode\":"
              "\"PREEMPTIVE_TRACING_MODE\"}");
  }

  {
    config.reset(
        new BackgroundTracingConfigImpl(BackgroundTracingConfig::PREEMPTIVE));

    std::unique_ptr<base::DictionaryValue> second_dict(
        new base::DictionaryValue());
    second_dict->SetString(
        "rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    second_dict->SetString("histogram_name", "foo");
    second_dict->SetInteger("histogram_lower_value", 1);

    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK\",\"configs\":[],\"mode\":"
              "\"PREEMPTIVE_TRACING_MODE\"}");
  }

  {
    config.reset(
        new BackgroundTracingConfigImpl(BackgroundTracingConfig::PREEMPTIVE));

    std::unique_ptr<base::DictionaryValue> second_dict(
        new base::DictionaryValue());
    second_dict->SetString(
        "rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    second_dict->SetString("histogram_name", "foo");
    second_dict->SetInteger("histogram_lower_value", 1);
    second_dict->SetInteger("histogram_upper_value", 1);

    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK\",\"configs\":[],\"mode\":"
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
    config.reset(
        new BackgroundTracingConfigImpl(BackgroundTracingConfig::REACTIVE));

    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString("rule", "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
    dict->SetString("trigger_name", "foo");
    config->AddReactiveRule(dict.get(),
                            BackgroundTracingConfigImpl::BENCHMARK_DEEP);

    EXPECT_EQ(ConfigToString(config.get()),
              "{\"configs\":[{\"category\":\"BENCHMARK_DEEP\",\"rule\":\"TRACE_"
              "ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\",\"trigger_delay\":30,"
              "\"trigger_name\":\"foo\"}],\"mode\":\"REACTIVE_TRACING_MODE\"}");
  }

  {
    config.reset(
        new BackgroundTracingConfigImpl(BackgroundTracingConfig::REACTIVE));

    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString("rule", "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
    dict->SetString("trigger_name", "foo1");
    config->AddReactiveRule(dict.get(),
                            BackgroundTracingConfigImpl::BENCHMARK_DEEP);

    dict->SetString("trigger_name", "foo2");
    config->AddReactiveRule(dict.get(),
                            BackgroundTracingConfigImpl::BENCHMARK_DEEP);

    EXPECT_EQ(
        ConfigToString(config.get()),
        "{\"configs\":[{\"category\":\"BENCHMARK_DEEP\",\"rule\":\"TRACE_"
        "ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL\",\"trigger_delay\":30,"
        "\"trigger_name\":\"foo1\"},{\"category\":\"BENCHMARK_DEEP\",\"rule\":"
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
      "\"category\": \"BENCHMARK\",\"trigger_delay\":30,"
      "\"trigger_name\": \"foo\"}],\"low_ram_buffer_size_kb\":800,"
      "\"medium_ram_buffer_size_kb\":1000,\"mobile_network_buffer_size_kb\":"
      "300,\"max_buffer_size_kb\":1000,\"upload_limit_kb\":500,"
      "\"upload_limit_network_kb\":600}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::REACTIVE);
  EXPECT_EQ(config->rules().size(), 1u);

  notifier.set_type(net::NetworkChangeNotifier::CONNECTION_2G);
#if defined(OS_ANDROID)
  EXPECT_EQ(300u, config->GetTraceConfig().GetTraceBufferSizeInKb());
  EXPECT_EQ(600u, config->GetTraceUploadLimitKb());
#endif

  notifier.set_type(net::NetworkChangeNotifier::CONNECTION_WIFI);
  EXPECT_LE(800u, config->GetTraceConfig().GetTraceBufferSizeInKb());
  EXPECT_EQ(500u, config->GetTraceUploadLimitKb());
}

}  // namespace content
