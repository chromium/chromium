// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_CONFIG_IMPL_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_CONFIG_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
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

  ~BackgroundTracingConfigImpl() override;

  // From BackgroundTracingConfig
  void IntoDict(base::DictionaryValue* dict) override;

  enum CategoryPreset {
    CATEGORY_PRESET_UNSET,
    CUSTOM_CATEGORY_PRESET,
    CUSTOM_TRACE_CONFIG,
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
    BENCHMARK_SERVICEWORKER,
    BENCHMARK_POWER,
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

  void AddPreemptiveRule(const base::DictionaryValue* dict);
  void AddReactiveRule(
      const base::DictionaryValue* dict,
      BackgroundTracingConfigImpl::CategoryPreset category_preset);
  void AddSystemRule(const base::DictionaryValue* dict);

  base::trace_event::TraceConfig GetTraceConfig() const;

  size_t GetTraceUploadLimitKb() const;
  int interning_reset_interval_ms() const {
    return interning_reset_interval_ms_;
  }

  void set_requires_anonymized_data(bool value) {
    requires_anonymized_data_ = value;
  }
  bool requires_anonymized_data() const { return requires_anonymized_data_; }

  static std::unique_ptr<BackgroundTracingConfigImpl> PreemptiveFromDict(
      const base::DictionaryValue* dict);
  static std::unique_ptr<BackgroundTracingConfigImpl> ReactiveFromDict(
      const base::DictionaryValue* dict);
  static std::unique_ptr<BackgroundTracingConfigImpl> SystemFromDict(
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

#if defined(OS_ANDROID)
  constexpr static int kMaxBufferSizeKb = 4 * 1024;
  // ~1MB compressed size.
  constexpr static int kUploadLimitKb = 5 * 1024;
#else
  constexpr static int kMaxBufferSizeKb = 25 * 1024;
  // Less than 10MB compressed size.
  constexpr static int kUploadLimitKb = 30 * 1024;
#endif

  static base::trace_event::TraceConfig GetConfigForCategoryPreset(
      BackgroundTracingConfigImpl::CategoryPreset,
      base::trace_event::TraceRecordMode);

  BackgroundTracingRule* AddRule(const base::DictionaryValue* dict);
  void SetBufferSizeLimits(const base::DictionaryValue* dict);
  int GetMaximumTraceBufferSizeKb() const;

  base::trace_event::TraceConfig trace_config_;
  CategoryPreset category_preset_;
  std::vector<std::unique_ptr<BackgroundTracingRule>> rules_;
  std::string scenario_name_;
  std::string custom_categories_;

  bool requires_anonymized_data_ = false;
  bool trace_browser_process_only_ = false;

  // The default memory overhead of running background tracing for various
  // scenarios. These are configurable by experiments.
  int low_ram_buffer_size_kb_ = 200;
  int medium_ram_buffer_size_kb_ = 2 * 1024;
  // Connectivity is also relevant for setting the buffer size because the
  // uploader will fail if we sent large trace and device runs on mobile
  // network.
  int mobile_network_buffer_size_kb_ = 300;
  int max_buffer_size_kb_ = kMaxBufferSizeKb;

  // All the upload limits below are set for uncompressed trace log. On
  // compression the data size usually reduces by 3x for size < 10MB, and the
  // compression ratio grows up to 8x if the buffer size is around 100MB.
  int upload_limit_network_kb_ = 1024;
  int upload_limit_kb_ = kUploadLimitKb;
  int interning_reset_interval_ms_ = 5000;

  DISALLOW_COPY_AND_ASSIGN(BackgroundTracingConfigImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_CONFIG_IMPL_H_
