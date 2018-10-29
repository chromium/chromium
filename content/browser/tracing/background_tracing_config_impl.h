// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_CONFIG_IMPL_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_CONFIG_IMPL_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/background_tracing_config.h"

namespace content {
class BackgroundTracingRule;

class CONTENT_EXPORT BackgroundTracingConfigImpl
    : public BackgroundTracingConfig {
 public:
  explicit BackgroundTracingConfigImpl(TracingMode tracing_mode);

  ~BackgroundTracingConfigImpl() override;

  // From BackgroundTracingConfig
  void IntoDict(base::DictionaryValue* dict) const override;

  enum CategoryPreset {
    CATEGORY_PRESET_UNSET,
    BENCHMARK,
    BENCHMARK_DEEP,
    BENCHMARK_GPU,
    BENCHMARK_IPC,
    BENCHMARK_STARTUP,
    BENCHMARK_BLINK_GC,
    BENCHMARK_MEMORY_HEAVY,
    BENCHMARK_MEMORY_LIGHT,
    BENCHMARK_EXECUTION_METRIC,
    BENCHMARK_NAVIGATION,
    BENCHMARK_RENDERERS,
    BLINK_STYLE
  };

  CategoryPreset category_preset() const { return category_preset_; }
  void set_category_preset(CategoryPreset category_preset) {
    category_preset_ = category_preset;
  }

  const std::vector<std::unique_ptr<BackgroundTracingRule>>& rules() const {
    return rules_;
  }
  const std::string& scenario_name() const { return scenario_name_; }
  const std::string& enable_blink_features() const {
    return enable_blink_features_;
  }
  const std::string& disable_blink_features() const {
    return disable_blink_features_;
  }

  void AddPreemptiveRule(const base::DictionaryValue* dict);
  void AddReactiveRule(
      const base::DictionaryValue* dict,
      BackgroundTracingConfigImpl::CategoryPreset category_preset);

  static std::unique_ptr<BackgroundTracingConfigImpl> PreemptiveFromDict(
      const base::DictionaryValue* dict);
  static std::unique_ptr<BackgroundTracingConfigImpl> ReactiveFromDict(
      const base::DictionaryValue* dict);

  static std::unique_ptr<BackgroundTracingConfigImpl> FromDict(
      const base::DictionaryValue* dict);

  static std::string CategoryPresetToString(
      BackgroundTracingConfigImpl::CategoryPreset category_preset);
  static bool StringToCategoryPreset(
      const std::string& category_preset_string,
      BackgroundTracingConfigImpl::CategoryPreset* category_preset);

 private:
  FRIEND_TEST_ALL_PREFIXES(BackgroundTracingConfigTest,
                           ValidPreemptiveConfigToString);

  CategoryPreset category_preset_;
  std::vector<std::unique_ptr<BackgroundTracingRule>> rules_;
  std::string scenario_name_;
  std::string enable_blink_features_;
  std::string disable_blink_features_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundTracingConfigImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_CONFIG_IMPL_H_
