// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/base_paths.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/test_proto_loader.h"
#include "base/time/time.h"
#include "base/trace_event/named_trigger.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/scenario_config.gen.h"

namespace content {

namespace {

class MockNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override { return type_; }
  void set_type(ConnectionType type) { type_ = type; }

 private:
  ConnectionType type_;
};

base::FilePath GetTestDataRoot() {
  return base::PathService::CheckedGet(base::DIR_GEN_TEST_DATA_ROOT);
}

void CreateRuleConfig(const std::string& proto_text,
                      perfetto::protos::gen::TriggerRule& destination) {
  base::TestProtoLoader loader(GetTestDataRoot().Append(FILE_PATH_LITERAL(
                                   "third_party/perfetto/protos/perfetto/"
                                   "config/chrome/scenario_config.descriptor")),
                               "perfetto.protos.TriggerRule");
  std::string serialized_message;
  loader.ParseFromText(proto_text, serialized_message);
  ASSERT_TRUE(destination.ParseFromString(serialized_message));
}

}  // namespace

class BackgroundTracingConfigTest : public testing::Test {
 public:
  BackgroundTracingConfigTest() = default;

 protected:
  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

std::unique_ptr<BackgroundTracingConfigImpl> ReadFromJSONString(
    const std::string& json_text) {
  std::optional<base::Value> json_value(base::JSONReader::Read(json_text));

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
                         "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\"}]}"));

  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", \"category\": "
      "[]}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", \"category\": "
      "\"\"}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", \"category\": "
      "\"benchmark\"}]}"));

  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", \"category\": "
      "\"benchmark\", \"trigger_name\": []}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", \"category\": "
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
            "\"histogram_upper_value\":2147483647,"
            "\"rule\":\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\"}");

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
            "\"histogram_upper_value\":2147483647,"
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
            "\"histogram_upper_value\":2,\"rule\":"
            "\"MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE\"}");

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
            "\"histogram_upper_value\":2,\"rule\":"
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
            "\"histogram_upper_value\":2147483647,"
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
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", "
      "\"trigger_delay\":30,"
      "\"trigger_name\": \"foo\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->tracing_mode(), BackgroundTracingConfig::REACTIVE);
  EXPECT_EQ(config->rules().size(), 1u);
  EXPECT_EQ(RuleToString(config->rules()[0]),
            "{\"rule\":\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\","
            "\"trigger_delay\":30,\"trigger_name\":\"foo\"}");
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

    auto dict = base::Value::Dict()
                    .Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED")
                    .Set("trigger_name", "foo");
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

    auto dict = base::Value::Dict()
                    .Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED")
                    .Set("trigger_name", "foo")
                    .Set("trigger_chance", 0.5);
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

    auto dict = base::Value::Dict()
                    .Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED")
                    .Set("trigger_name", "foo1");
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

    auto second_dict =
        base::Value::Dict()
            .Set("rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE")
            .Set("histogram_name", "foo")
            .Set("histogram_lower_value", 1)
            .Set("histogram_upper_value", 2);
    config->AddPreemptiveRule(second_dict);

    EXPECT_EQ(
        ConfigToString(config.get()),
        "{\"category\":\"BENCHMARK_STARTUP\",\"configs\":[{\"histogram_lower_"
        "value\":1,\"histogram_name\":\"foo\","
        "\"histogram_upper_value\":2,\"rule\":\"MONITOR_AND_DUMP_WHEN_"
        "SPECIFIC_HISTOGRAM_AND_VALUE\"}],\"mode\":\"PREEMPTIVE_TRACING_"
        "MODE\"}");
  }

  {
    config = std::make_unique<BackgroundTracingConfigImpl>(
        BackgroundTracingConfig::PREEMPTIVE);

    auto second_dict =
        base::Value::Dict()
            .Set("rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE")
            .Set("histogram_name", "foo")
            .Set("histogram_lower_value", 1)
            .Set("histogram_upper_value", 2)
            .Set("trigger_delay", 10);
    config->AddPreemptiveRule(second_dict);

    EXPECT_EQ(
        ConfigToString(config.get()),
        "{\"category\":\"BENCHMARK_STARTUP\",\"configs\":[{\"histogram_lower_"
        "value\":1,\"histogram_name\":\"foo\","
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

    auto dict = base::Value::Dict().Set(
        "rule", "MONITOR_AND_DUMP_WHEN_BROWSER_STARTUP_COMPLETE");
    config->AddPreemptiveRule(dict);

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

    auto dict = base::Value::Dict()
                    .Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED")
                    .Set("trigger_name", "foo");
    config->AddReactiveRule(dict);

    EXPECT_EQ(ConfigToString(config.get()),
              "{\"configs\":[{\"rule\":\""
              "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\","
              "\"trigger_name\":\"foo\"}],\"mode\":\"REACTIVE_TRACING_MODE\"}");
  }

  {
    config = std::make_unique<BackgroundTracingConfigImpl>(
        BackgroundTracingConfig::REACTIVE);

    auto dict = base::Value::Dict()
                    .Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED")
                    .Set("trigger_name", "foo1");
    config->AddReactiveRule(dict);

    dict.Set("trigger_name", "foo2");
    config->AddReactiveRule(dict);

    EXPECT_EQ(
        ConfigToString(config.get()),
        "{\"configs\":[{\"rule\":"
        "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\","
        "\"trigger_name\":\"foo1\"},{"
        "\"rule\":\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\","
        "\"trigger_name\":\"foo2\"}],\"mode\":\"REACTIVE_TRACING_MODE\"}");
  }
}

TEST_F(BackgroundTracingConfigTest, BufferLimitConfig) {
  MockNetworkChangeNotifier notifier;

  std::unique_ptr<BackgroundTracingConfigImpl> config;

  config = ReadFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", "
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
  EXPECT_EQ(600u, config->upload_limit_network_kb());
#endif

  notifier.set_type(net::NetworkChangeNotifier::CONNECTION_WIFI);
  EXPECT_LE(800u, config->GetTraceConfig().GetTraceBufferSizeInKb());
  EXPECT_EQ(500u, config->upload_limit_kb());
}

TEST_F(BackgroundTracingConfigTest, HistogramRuleFromValidProto) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(
      R"pb(
        name: "test_rule"
        trigger_chance: 0.5
        delay_ms: 500
        histogram: { histogram_name: "foo" min_value: 1 max_value: 2 }
      )pb",
      config);
  auto rule = BackgroundTracingRule::Create(config);
  auto result = rule->ToProtoForTesting();
  EXPECT_EQ("test_rule", result.name());
  EXPECT_EQ(0.5, result.trigger_chance());
  EXPECT_EQ(500U, result.delay_ms());
  EXPECT_TRUE(result.has_histogram());
  EXPECT_EQ("foo", result.histogram().histogram_name());
  EXPECT_EQ(1, result.histogram().min_value());
  EXPECT_EQ(2, result.histogram().max_value());
}

TEST_F(BackgroundTracingConfigTest, NamedRuleFromValidProto) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(R"pb(
                     name: "test_rule"
                     trigger_chance: 0.5
                     delay_ms: 500
                     manual_trigger_name: "test_trigger"
                   )pb",
                   config);
  auto rule = BackgroundTracingRule::Create(config);
  auto result = rule->ToProtoForTesting();
  EXPECT_EQ("test_rule", result.name());
  EXPECT_EQ(0.5, result.trigger_chance());
  EXPECT_EQ(500U, result.delay_ms());
  EXPECT_EQ("test_trigger", result.manual_trigger_name());
}

TEST_F(BackgroundTracingConfigTest, RuleFromEmptyProto) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(R"pb(
                     name: "test_rule"
                   )pb",
                   config);
  auto rule = BackgroundTracingRule::Create(config);
  EXPECT_EQ(nullptr, rule);
}

TEST_F(BackgroundTracingConfigTest, TimerRuleFromValidProto) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(R"pb(
                     name: "test_rule" trigger_chance: 0.5 delay_ms: 500
                   )pb",
                   config);
  auto rule = BackgroundTracingRule::Create(config);
  auto result = rule->ToProtoForTesting();
  EXPECT_EQ("test_rule", result.name());
  EXPECT_EQ(0.5, result.trigger_chance());
  EXPECT_EQ(500U, result.delay_ms());
}

TEST_F(BackgroundTracingConfigTest, TimerRuleTriggersAfterDelay) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(R"pb(
                     name: "test_rule" delay_ms: 10000
                   )pb",
                   config);

  base::TimeTicks start = base::TimeTicks::Now();
  auto rule = BackgroundTracingRule::Create(config);
  base::RunLoop run_loop;
  rule->Install(base::BindLambdaForTesting([&](const BackgroundTracingRule*) {
    run_loop.Quit();
    return true;
  }));
  run_loop.Run();
  DCHECK_GE(base::TimeTicks::Now(), start + base::Milliseconds(10000));
  rule->Uninstall();
}

TEST_F(BackgroundTracingConfigTest, RuleActivatesAfterDelay) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(R"pb(
                     name: "test_rule"
                     manual_trigger_name: "test_rule"
                     activation_delay_ms: 10000
                   )pb",
                   config);

  std::unique_ptr<content::BackgroundTracingManager>
      background_tracing_manager =
          content::BackgroundTracingManager::CreateInstance();

  auto rule = BackgroundTracingRule::Create(config);

  base::RunLoop run_loop;
  rule->Install(base::BindLambdaForTesting([&](const BackgroundTracingRule*) {
    run_loop.Quit();
    return true;
  }));

  // Rule is not activated yet.
  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("test_rule"));
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("test_rule"));
  run_loop.Run();
  rule->Uninstall();
}

TEST_F(BackgroundTracingConfigTest, RepeatingIntervalRuleFromValidProto) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(
      R"pb(
        name: "test_rule"
        trigger_chance: 0.5
        delay_ms: 500
        repeating_interval: { period_ms: 1000 randomized: true }
      )pb",
      config);
  auto rule = BackgroundTracingRule::Create(config);
  auto result = rule->ToProtoForTesting();
  EXPECT_EQ("test_rule", result.name());
  EXPECT_EQ(0.5, result.trigger_chance());
  EXPECT_EQ(500U, result.delay_ms());
  EXPECT_TRUE(result.has_repeating_interval());
  EXPECT_EQ(1000U, result.repeating_interval().period_ms());
  EXPECT_TRUE(result.repeating_interval().randomized());
}

TEST_F(BackgroundTracingConfigTest, RepeatingIntervalRuleTriggersAfterDelay) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(R"pb(
                     name: "test_rule"
                     repeating_interval: { period_ms: 2000 }
                   )pb",
                   config);

  base::TimeTicks start = base::TimeTicks::Now();
  auto rule = BackgroundTracingRule::Create(config);
  std::vector<base::TimeTicks> trigger_times;
  auto callback = base::BindLambdaForTesting([&](const BackgroundTracingRule*) {
    trigger_times.push_back(base::TimeTicks::Now());
    return true;
  });
  rule->Install(callback);
  task_environment_.FastForwardBy(base::Seconds(2));  // Triggers at 2s
  rule->Uninstall();
  task_environment_.FastForwardBy(base::Seconds(1));
  rule->Install(callback);
  task_environment_.FastForwardBy(base::Seconds(2));  // Triggers at 4s
  rule->Uninstall();
  task_environment_.FastForwardBy(base::Seconds(2));  // Skips 6s
  rule->Install(callback);
  task_environment_.FastForwardBy(base::Seconds(1));  // Triggers at 8s

  EXPECT_EQ(std::vector<base::TimeTicks>({
                start + base::Seconds(2),
                start + base::Seconds(4),
                start + base::Seconds(8),
            }),
            trigger_times);
  rule->Uninstall();
}

TEST_F(BackgroundTracingConfigTest, RepeatingIntervalRuleTriggersRandomized) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(
      R"pb(
        name: "test_rule"
        repeating_interval: { period_ms: 2000 randomized: true }
      )pb",
      config);

  base::TimeTicks start = base::TimeTicks::Now();
  auto rule = BackgroundTracingRule::Create(config);
  std::vector<base::TimeTicks> trigger_times;
  auto callback = base::BindLambdaForTesting([&](const BackgroundTracingRule*) {
    trigger_times.push_back(base::TimeTicks::Now());
    return true;
  });
  rule->Install(callback);
  task_environment_.FastForwardBy(base::Seconds(4));  // Triggers twice
  ASSERT_EQ(2U, trigger_times.size());
  EXPECT_GE(trigger_times[0], start);
  EXPECT_LT(trigger_times[0], trigger_times[1]);
  EXPECT_GE(trigger_times[1], start + base::Seconds(2));
  rule->Uninstall();
}

}  // namespace content
