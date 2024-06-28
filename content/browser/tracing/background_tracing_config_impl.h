// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_CONFIG_IMPL_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_CONFIG_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/trace_event/trace_config.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/background_tracing_config.h"

namespace content {
class BackgroundTracingRule;

class CONTENT_EXPORT BackgroundTracingConfigImpl
    : public BackgroundTracingConfig {
 public:
  explicit BackgroundTracingConfigImpl(TracingMode tracing_mode);

  BackgroundTracingConfigImpl(const BackgroundTracingConfigImpl&) = delete;
  BackgroundTracingConfigImpl& operator=(const BackgroundTracingConfigImpl&) =
      delete;

  ~BackgroundTracingConfigImpl() override;

  // From BackgroundTracingConfig
  base::Value::Dict ToDict() override;

  void SetPackageNameFilteringEnabled(bool) override;

  enum CategoryPreset {
    CATEGORY_PRESET_UNSET,
    CUSTOM_CATEGORY_PRESET,
    CUSTOM_TRACE_CONFIG,
    BENCHMARK_STARTUP,
  };

  CategoryPreset category_preset() const { return category_preset_; }
  void set_category_preset(CategoryPreset category_preset) {
    category_preset_ = category_preset;
  }

  const std::vector<std::unique_ptr<BackgroundTracingRule>>& rules() const {
    return rules_;
  }

  void AddPreemptiveRule(const base::Value::Dict& dict);
  void AddReactiveRule(const base::Value::Dict& dict);
  void AddSystemRule(const base::Value::Dict& dict);

  base::trace_event::TraceConfig GetTraceConfig() const;
  const std::string& enabled_data_sources() const {
    return enabled_data_sources_;
  }

  int interning_reset_interval_ms() const {
    return interning_reset_interval_ms_;
  }

  void set_requires_anonymized_data(bool value) {
    requires_anonymized_data_ = value;
  }
  bool requires_anonymized_data() const { return requires_anonymized_data_; }

  std::optional<size_t> upload_limit_network_kb() const {
    return upload_limit_network_kb_;
  }
  std::optional<size_t> upload_limit_kb() const { return upload_limit_kb_; }

  static std::unique_ptr<BackgroundTracingConfigImpl> PreemptiveFromDict(
      const base::Value::Dict& dict);
  static std::unique_ptr<BackgroundTracingConfigImpl> ReactiveFromDict(
      const base::Value::Dict& dict);
  static std::unique_ptr<BackgroundTracingConfigImpl> SystemFromDict(
      const base::Value::Dict& dict);

  static std::unique_ptr<BackgroundTracingConfigImpl> FromDict(
      base::Value::Dict&& dict);

  static std::string CategoryPresetToString(
      BackgroundTracingConfigImpl::CategoryPreset category_preset);
  static bool StringToCategoryPreset(
      const std::string& category_preset_string,
      BackgroundTracingConfigImpl::CategoryPreset* category_preset);

 private:
  FRIEND_TEST_ALL_PREFIXES(BackgroundTracingConfigTest,
                           ValidPreemptiveConfigToString);

#if BUILDFLAG(IS_ANDROID)
  constexpr static int kMaxBufferSizeKb = 4 * 1024;
#else
  constexpr static int kMaxBufferSizeKb = 25 * 1024;
#endif

  BackgroundTracingRule* AddRule(const base::Value::Dict& dict);
  void SetBufferSizeLimits(const base::Value::Dict* dict);
  int GetMaximumTraceBufferSizeKb() const;

  // A trace config extracted from the "trace_config" field of the input
  // dictionnary.
  base::trace_event::TraceConfig trace_config_;
  CategoryPreset category_preset_;
  std::vector<std::unique_ptr<BackgroundTracingRule>> rules_;
  std::string custom_categories_;
  std::string enabled_data_sources_;

  bool requires_anonymized_data_ = false;

  // The default memory overhead of running background tracing for various
  // scenarios. These are configurable by experiments.
  int low_ram_buffer_size_kb_ = 200;
  int medium_ram_buffer_size_kb_ = 2 * 1024;
  // Connectivity is also relevant for setting the buffer size because the
  // uploader will fail if we sent large trace and device runs on mobile
  // network.
  int mobile_network_buffer_size_kb_ = 300;
  int max_buffer_size_kb_ = kMaxBufferSizeKb;

  std::optional<size_t> upload_limit_network_kb_;
  std::optional<size_t> upload_limit_kb_;
  int interning_reset_interval_ms_ = 5000;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_CONFIG_IMPL_H_
