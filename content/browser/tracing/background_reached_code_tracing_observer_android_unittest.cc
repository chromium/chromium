// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_reached_code_tracing_observer_android.h"

#include "base/android/reached_code_profiler.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/values.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

bool EnableReachedCodeProfiler() {
  if (!base::android::IsReachedCodeProfilerSupported())
    return false;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableReachedCodeProfiler);
  base::android::InitReachedCodeProfilerAtStartup(
      base::android::PROCESS_BROWSER);
  EXPECT_TRUE(base::android::IsReachedCodeProfilerEnabled);
  return true;
}

const BackgroundTracingRule* FindReachedCodeRuleInConfig(
    const BackgroundTracingConfigImpl& config) {
  for (const auto& rule : config.rules()) {
    if (rule->rule_id().find("reached-code-config") != std::string::npos) {
      return rule.get();
    }
  }
  return nullptr;
}

std::unique_ptr<BackgroundTracingConfigImpl> GetStartupConfig() {
  base::Value rules_dict(base::Value::Type::DICTIONARY);
  rules_dict.SetStringKey("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
  rules_dict.SetStringKey("trigger_name", "test");
  rules_dict.SetStringKey("category", "BENCHMARK_STARTUP");
  base::Value dict(base::Value::Type::DICTIONARY);
  base::Value rules_list(base::Value::Type::LIST);
  rules_list.Append(std::move(rules_dict));
  dict.SetKey("configs", std::move(rules_list));
  return BackgroundTracingConfigImpl::ReactiveFromDict(dict);
}

std::unique_ptr<BackgroundTracingConfigImpl> GetReachedCodeConfig() {
  base::Value rules_dict(base::Value::Type::DICTIONARY);
  rules_dict.SetStringKey("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
  rules_dict.SetStringKey("trigger_name", "reached-code-config");
  rules_dict.SetIntKey("trigger_delay", 30);

  base::Value dict(base::Value::Type::DICTIONARY);
  base::Value rules_list(base::Value::Type::LIST);
  rules_list.Append(std::move(rules_dict));
  dict.SetKey("configs", std::move(rules_list));
  dict.SetStringKey(
      "enabled_data_sources",
      "org.chromium.trace_metadata,org.chromium.reached_code_profiler");
  dict.SetStringKey("category", "CUSTOM");
  dict.SetStringKey("custom_categories", "-*");
  auto config = BackgroundTracingConfigImpl::ReactiveFromDict(dict);
  EXPECT_TRUE(FindReachedCodeRuleInConfig(*config));
  return config;
}

void TestReachedCodeRuleExists(const BackgroundTracingConfigImpl& config,
                               bool exists) {
  const auto* rule = FindReachedCodeRuleInConfig(config);
  if (exists) {
    ASSERT_TRUE(rule);
    EXPECT_EQ(30, rule->GetTraceDelay());
    EXPECT_FALSE(rule->stop_tracing_on_repeated_reactive());
    EXPECT_EQ("org.chromium.trace_metadata,org.chromium.reached_code_profiler",
              config.enabled_data_sources());
  } else {
    EXPECT_FALSE(rule);
  }
}

void TestStartupConfigExists(const BackgroundTracingConfigImpl& config) {
  bool found_startup = false;
  for (const auto& rule : config.rules()) {
    if (rule->category_preset() ==
        BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_STARTUP) {
      found_startup = true;
    }
  }
  EXPECT_TRUE(found_startup);
}

}  // namespace

TEST(BackgroundReachedCodeTracingObserverTest,
     IncludeReachedCodeConfigIfNeeded) {
  EXPECT_FALSE(base::android::IsReachedCodeProfilerEnabled());
  BackgroundReachedCodeTracingObserver& observer =
      BackgroundReachedCodeTracingObserver::GetInstance();

  // Empty config without profiler set should not do anything.
  std::unique_ptr<content::BackgroundTracingConfigImpl> config_impl;
  config_impl = observer.IncludeReachedCodeConfigIfNeeded(nullptr);
  EXPECT_FALSE(config_impl);
  EXPECT_FALSE(observer.enabled_in_current_session());
  EXPECT_FALSE(base::android::IsReachedCodeProfilerEnabled());

  // A startup config without preference set should not set preference and keep
  // config same.
  config_impl = GetStartupConfig();
  ASSERT_TRUE(config_impl);

  EXPECT_FALSE(base::android::IsReachedCodeProfilerEnabled());
  config_impl =
      observer.IncludeReachedCodeConfigIfNeeded(std::move(config_impl));
  EXPECT_FALSE(observer.enabled_in_current_session());
  EXPECT_FALSE(base::android::IsReachedCodeProfilerEnabled());
  EXPECT_EQ(1u, config_impl->rules().size());
  TestReachedCodeRuleExists(*config_impl, false);
  TestStartupConfigExists(*config_impl);

  // A reached code config without profiler should stay config same.
  config_impl = GetReachedCodeConfig();
  config_impl =
      observer.IncludeReachedCodeConfigIfNeeded(std::move(config_impl));
  EXPECT_FALSE(observer.enabled_in_current_session());
  ASSERT_TRUE(config_impl);
  EXPECT_FALSE(base::android::IsReachedCodeProfilerEnabled());
  EXPECT_EQ(1u, config_impl->rules().size());
  TestReachedCodeRuleExists(*config_impl, true);

  if (!base::android::IsReachedCodeProfilerSupported())
    return;
  config_impl.reset();
  EXPECT_TRUE(EnableReachedCodeProfiler());
  BackgroundReachedCodeTracingObserver::ResetForTesting();
  EXPECT_TRUE(observer.enabled_in_current_session());

  // Empty config with profiler should create a config.
  EXPECT_TRUE(base::android::IsReachedCodeProfilerEnabled());
  config_impl =
      observer.IncludeReachedCodeConfigIfNeeded(std::move(config_impl));
  EXPECT_TRUE(base::android::IsReachedCodeProfilerEnabled());
  EXPECT_TRUE(observer.enabled_in_current_session());
  ASSERT_TRUE(config_impl);
  EXPECT_EQ(1u, config_impl->rules().size());
  EXPECT_EQ(BackgroundTracingConfig::TracingMode::REACTIVE,
            config_impl->tracing_mode());
  TestReachedCodeRuleExists(*config_impl, true);

  // A startup config with profiler on should not enabled reached code config.
  config_impl = GetStartupConfig();
  config_impl =
      observer.IncludeReachedCodeConfigIfNeeded(std::move(config_impl));
  EXPECT_FALSE(observer.enabled_in_current_session());
  ASSERT_TRUE(config_impl);
  EXPECT_TRUE(base::android::IsReachedCodeProfilerEnabled());
  EXPECT_EQ(1u, config_impl->rules().size());
  EXPECT_EQ(BackgroundTracingConfig::TracingMode::REACTIVE,
            config_impl->tracing_mode());
  TestReachedCodeRuleExists(*config_impl, false);

  TestStartupConfigExists(*config_impl);
}

}  // namespace content
