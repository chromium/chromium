// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_startup_tracing_observer.h"

#include "build/build_config.h"
#include "components/tracing/common/trace_startup_config.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class TestPreferenceManagerImpl
    : public BackgroundStartupTracingObserver::PreferenceManager {
 public:
  void SetBackgroundStartupTracingEnabled(bool enabled) override {
    enabled_ = enabled;
  }

  bool GetBackgroundStartupTracingEnabled() const override { return enabled_; }

 private:
  bool enabled_ = false;
};

void TestStartupRuleExists(const BackgroundTracingConfigImpl& config,
                           bool exists) {
  const auto* rule =
      BackgroundStartupTracingObserver::FindStartupRuleInConfig(config);
  if (exists) {
    ASSERT_TRUE(rule);
    EXPECT_EQ(BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_STARTUP,
              rule->category_preset());
    EXPECT_EQ(30, rule->GetTraceDelay());
  } else {
    EXPECT_FALSE(rule);
  }
}

}  // namespace

TEST(BackgroundStartupTracingObserverTest, IncludeStartupConfigIfNeeded) {
  BackgroundStartupTracingObserver& observer =
      BackgroundStartupTracingObserver::GetInstance();
  std::unique_ptr<TestPreferenceManagerImpl> test_preferences(
      new TestPreferenceManagerImpl);
  TestPreferenceManagerImpl* preferences = test_preferences.get();
  observer.SetPreferenceManagerForTesting(std::move(test_preferences));

  // Empty config without preference set should not do anything.
  std::unique_ptr<content::BackgroundTracingConfigImpl> config_impl;
  config_impl = observer.IncludeStartupConfigIfNeeded(std::move(config_impl));
  EXPECT_FALSE(config_impl);
  EXPECT_FALSE(observer.enabled_in_current_session());

  // Empty config with preference set should create a startup config, and reset
  // preference.
  EXPECT_FALSE(preferences->GetBackgroundStartupTracingEnabled());
  preferences->SetBackgroundStartupTracingEnabled(true);
  config_impl = observer.IncludeStartupConfigIfNeeded(std::move(config_impl));
  EXPECT_TRUE(observer.enabled_in_current_session());
  EXPECT_FALSE(preferences->GetBackgroundStartupTracingEnabled());
  ASSERT_TRUE(config_impl);
  EXPECT_EQ(1u, config_impl->rules().size());
  EXPECT_EQ(BackgroundTracingConfig::TracingMode::REACTIVE,
            config_impl->tracing_mode());
  TestStartupRuleExists(*config_impl, true);

  // Startup config with preference set should keep config and preference same.
  preferences->SetBackgroundStartupTracingEnabled(true);
  config_impl = observer.IncludeStartupConfigIfNeeded(std::move(config_impl));
  EXPECT_TRUE(observer.enabled_in_current_session());
  EXPECT_TRUE(preferences->GetBackgroundStartupTracingEnabled());
  ASSERT_TRUE(config_impl);
  EXPECT_EQ(1u, config_impl->rules().size());
  TestStartupRuleExists(*config_impl, true);

  // Startup config without preference set should keep config and set
  // preference.
  preferences->SetBackgroundStartupTracingEnabled(false);
  config_impl = observer.IncludeStartupConfigIfNeeded(std::move(config_impl));
  EXPECT_FALSE(observer.enabled_in_current_session());
  EXPECT_TRUE(preferences->GetBackgroundStartupTracingEnabled());
  ASSERT_TRUE(config_impl);
  EXPECT_EQ(1u, config_impl->rules().size());
  TestStartupRuleExists(*config_impl, true);

  // A custom config without preference set should not set preference and keep
  // config same.
  base::Value::Dict rules_dict;
  rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
  rules_dict.Set("trigger_name", "test");
  base::Value::Dict dict;
  base::Value::List rules_list;
  rules_list.Append(std::move(rules_dict));
  dict.Set("configs", std::move(rules_list));
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);
  config_impl = BackgroundTracingConfigImpl::ReactiveFromDict(dict);
  ASSERT_TRUE(config_impl);

  preferences->SetBackgroundStartupTracingEnabled(false);
  config_impl = observer.IncludeStartupConfigIfNeeded(std::move(config_impl));
  EXPECT_FALSE(observer.enabled_in_current_session());
  EXPECT_FALSE(preferences->GetBackgroundStartupTracingEnabled());
  EXPECT_EQ(1u, config_impl->rules().size());
  TestStartupRuleExists(*config_impl, false);

  // A custom config with preference set should include startup config and
  // disable preference.
  preferences->SetBackgroundStartupTracingEnabled(true);
  config_impl = observer.IncludeStartupConfigIfNeeded(std::move(config_impl));
  EXPECT_TRUE(observer.enabled_in_current_session());
  EXPECT_FALSE(preferences->GetBackgroundStartupTracingEnabled());
  ASSERT_TRUE(config_impl);
  EXPECT_EQ(2u, config_impl->rules().size());
  EXPECT_EQ(BackgroundTracingConfig::TracingMode::REACTIVE,
            config_impl->tracing_mode());
  TestStartupRuleExists(*config_impl, true);
  EXPECT_EQ(
      config_impl->category_preset(),
      BackgroundTracingConfigImpl::CategoryPreset::CUSTOM_CATEGORY_PRESET);

  preferences->SetBackgroundStartupTracingEnabled(false);
}

}  // namespace content
