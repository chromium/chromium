// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_tracing_config_impl.h"

#include <set>
#include <utility>

#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_handle.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "net/base/network_change_notifier.h"
#include "services/tracing/public/cpp/trace_startup_config.h"

using base::trace_event::TraceConfig;

namespace content {

namespace {

const char kConfigsKey[] = "configs";

const char kConfigModeKey[] = "mode";
const char kConfigModePreemptive[] = "PREEMPTIVE_TRACING_MODE";
const char kConfigModeReactive[] = "REACTIVE_TRACING_MODE";
const char kConfigModeSystem[] = "SYSTEM_TRACING_MODE";

const char kConfigScenarioName[] = "scenario_name";
const char kEnabledDataSourcesKey[] = "enabled_data_sources";

const char kConfigCategoryKey[] = "category";
const char kConfigCustomCategoriesKey[] = "custom_categories";
const char kConfigTraceConfigKey[] = "trace_config";
const char kConfigCategoryBenchmarkStartup[] = "BENCHMARK_STARTUP";
const char kConfigCategoryCustom[] = "CUSTOM";
const char kConfigCustomConfig[] = "CUSTOM_CONFIG";

const char kConfigLowRamBufferSizeKb[] = "low_ram_buffer_size_kb";
const char kConfigMediumRamBufferSizeKb[] = "medium_ram_buffer_size_kb";
const char kConfigMobileNetworkBuferSizeKb[] = "mobile_network_buffer_size_kb";
const char kConfigMaxBufferSizeKb[] = "max_buffer_size_kb";
const char kConfigUploadLimitKb[] = "upload_limit_kb";
const char kConfigUploadLimitNetworkKb[] = "upload_limit_network_kb";
const char kConfigInterningResetIntervalMs[] = "interning_reset_interval_ms";

}  // namespace

BackgroundTracingConfigImpl::BackgroundTracingConfigImpl(
    TracingMode tracing_mode)
    : BackgroundTracingConfig(tracing_mode),
      category_preset_(BackgroundTracingConfigImpl::BENCHMARK_STARTUP) {}

BackgroundTracingConfigImpl::~BackgroundTracingConfigImpl() = default;

// static
std::string BackgroundTracingConfigImpl::CategoryPresetToString(
    BackgroundTracingConfigImpl::CategoryPreset category_preset) {
  switch (category_preset) {
    case BackgroundTracingConfigImpl::BENCHMARK_STARTUP:
      return kConfigCategoryBenchmarkStartup;
    case BackgroundTracingConfigImpl::CUSTOM_CATEGORY_PRESET:
      return kConfigCategoryCustom;
    case BackgroundTracingConfigImpl::CUSTOM_TRACE_CONFIG:
      return kConfigCustomConfig;
    case BackgroundTracingConfigImpl::CATEGORY_PRESET_UNSET:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

// static
bool BackgroundTracingConfigImpl::StringToCategoryPreset(
    const std::string& category_preset_string,
    BackgroundTracingConfigImpl::CategoryPreset* category_preset) {
  if (category_preset_string == kConfigCategoryBenchmarkStartup) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_STARTUP;
    return true;
  }

  return false;
}

base::Value::Dict BackgroundTracingConfigImpl::ToDict() {
  base::Value::Dict dict;

  if (category_preset_ == CUSTOM_CATEGORY_PRESET) {
    dict.Set(kConfigCustomCategoriesKey, custom_categories_);
  } else if (category_preset_ == CUSTOM_TRACE_CONFIG) {
    std::optional<base::Value> trace_config =
        base::JSONReader::Read(trace_config_.ToString());
    if (trace_config) {
      dict.Set(kConfigTraceConfigKey, std::move(*trace_config));
    }
  }
  if (!enabled_data_sources_.empty()) {
    dict.Set(kEnabledDataSourcesKey, enabled_data_sources_);
  }

  switch (tracing_mode()) {
    case BackgroundTracingConfigImpl::PREEMPTIVE:
      dict.Set(kConfigModeKey, kConfigModePreemptive);
      dict.Set(kConfigCategoryKey, CategoryPresetToString(category_preset_));
      break;
    case BackgroundTracingConfigImpl::REACTIVE:
      dict.Set(kConfigModeKey, kConfigModeReactive);
      break;
    case BackgroundTracingConfigImpl::SYSTEM:
      dict.Set(kConfigModeKey, kConfigModeSystem);
      break;
  }

  base::Value::List configs_list;
  for (const auto& rule : rules_) {
    DCHECK(rule);
    configs_list.Append(rule->ToDict());
  }

  dict.Set(kConfigsKey, std::move(configs_list));

  if (!scenario_name_.empty())
    dict.Set(kConfigScenarioName, scenario_name_);

  return dict;
}

void BackgroundTracingConfigImpl::SetPackageNameFilteringEnabled(bool enabled) {
  trace_config_.SetEventPackageNameFilterEnabled(enabled);
}

void BackgroundTracingConfigImpl::AddPreemptiveRule(
    const base::Value::Dict& dict) {
  AddRule(dict);
}

void BackgroundTracingConfigImpl::AddReactiveRule(
    const base::Value::Dict& dict) {
  AddRule(dict);
}

void BackgroundTracingConfigImpl::AddSystemRule(const base::Value::Dict& dict) {
  AddRule(dict);
}

TraceConfig BackgroundTracingConfigImpl::GetTraceConfig() const {
  base::trace_event::TraceRecordMode record_mode =
      (tracing_mode() == BackgroundTracingConfigImpl::REACTIVE)
          ? base::trace_event::RECORD_UNTIL_FULL
          : base::trace_event::RECORD_CONTINUOUSLY;

  TraceConfig chrome_config;
  if (category_preset() == CUSTOM_TRACE_CONFIG) {
    chrome_config = trace_config_;
    if (!chrome_config.process_filter_config().included_process_ids().empty()) {
      // |included_process_ids| are not allowed in BackgroundTracing because
      // PIDs can't be known ahead of time.
      chrome_config.SetProcessFilterConfig(TraceConfig::ProcessFilterConfig());
    }
  } else if (category_preset() == CUSTOM_CATEGORY_PRESET) {
    chrome_config = TraceConfig(custom_categories_, record_mode);
  }

  chrome_config.SetTraceBufferSizeInKb(GetMaximumTraceBufferSizeKb());
  chrome_config.SetEventPackageNameFilterEnabled(
      trace_config_.IsEventPackageNameFilterEnabled());

#if BUILDFLAG(IS_ANDROID)
  // For legacy tracing backend, set low trace buffer size on Android in order
  // to upload small trace files.
  if (tracing_mode() == BackgroundTracingConfigImpl::PREEMPTIVE) {
    chrome_config.SetTraceBufferSizeInEvents(20000);
  }
#endif

  return chrome_config;
}

// static
std::unique_ptr<BackgroundTracingConfigImpl>
BackgroundTracingConfigImpl::FromDict(base::Value::Dict&& dict) {
  const std::string* mode = dict.FindString(kConfigModeKey);
  if (!mode)
    return nullptr;

  std::unique_ptr<BackgroundTracingConfigImpl> config;

  if (*mode == kConfigModePreemptive) {
    config = PreemptiveFromDict(dict);
  } else if (*mode == kConfigModeReactive) {
    config = ReactiveFromDict(dict);
  } else if (*mode == kConfigModeSystem) {
    config = SystemFromDict(dict);
  } else {
    return nullptr;
  }

  if (config) {
    if (const std::string* scenario = dict.FindString(kConfigScenarioName)) {
      config->scenario_name_ = *scenario;
    }
    config->SetBufferSizeLimits(&dict);
  }

  return config;
}

// static
std::unique_ptr<BackgroundTracingConfigImpl>
BackgroundTracingConfigImpl::PreemptiveFromDict(const base::Value::Dict& dict) {
  std::unique_ptr<BackgroundTracingConfigImpl> config(
      new BackgroundTracingConfigImpl(BackgroundTracingConfigImpl::PREEMPTIVE));

  if (const base::Value::Dict* trace_config =
          dict.FindDict(kConfigTraceConfigKey)) {
    config->trace_config_ = TraceConfig(trace_config->Clone());
    config->category_preset_ = CUSTOM_TRACE_CONFIG;
  } else if (const std::string* categories =
                 dict.FindString(kConfigCustomCategoriesKey)) {
    config->custom_categories_ = *categories;
    config->category_preset_ = CUSTOM_CATEGORY_PRESET;
  } else {
    const std::string* category_preset_string =
        dict.FindString(kConfigCategoryKey);
    if (!category_preset_string)
      return nullptr;

    if (!StringToCategoryPreset(*category_preset_string,
                                &config->category_preset_)) {
      return nullptr;
    }
  }
  if (const std::string* enabled_data_sources =
          dict.FindString(kEnabledDataSourcesKey)) {
    config->enabled_data_sources_ = *enabled_data_sources;
  }

  const base::Value::List* configs_list = dict.FindList(kConfigsKey);
  if (!configs_list)
    return nullptr;

  for (const auto& config_dict : *configs_list) {
    if (!config_dict.is_dict())
      return nullptr;

    config->AddPreemptiveRule(config_dict.GetDict());
  }

  if (config->rules().empty())
    return nullptr;

  return config;
}

// static
std::unique_ptr<BackgroundTracingConfigImpl>
BackgroundTracingConfigImpl::ReactiveFromDict(const base::Value::Dict& dict) {
  std::unique_ptr<BackgroundTracingConfigImpl> config(
      new BackgroundTracingConfigImpl(BackgroundTracingConfigImpl::REACTIVE));

  bool has_global_categories = false;
  if (const base::Value::Dict* trace_config =
          dict.FindDict(kConfigTraceConfigKey)) {
    config->trace_config_ = TraceConfig(trace_config->Clone());
    config->category_preset_ = CUSTOM_TRACE_CONFIG;
    has_global_categories = true;
  } else if (const std::string* categories =
                 dict.FindString(kConfigCustomCategoriesKey)) {
    config->custom_categories_ = *categories;
    config->category_preset_ = CUSTOM_CATEGORY_PRESET;
    has_global_categories = true;
  } else if (const std::string* category_preset_string =
                 dict.FindString(kConfigCategoryKey)) {
    if (!StringToCategoryPreset(*category_preset_string,
                                &config->category_preset_)) {
      return nullptr;
    }
    has_global_categories = true;
  }

  if (const std::string* enabled_data_sources =
          dict.FindString(kEnabledDataSourcesKey)) {
    config->enabled_data_sources_ = *enabled_data_sources;
  }

  const base::Value::List* configs_list = dict.FindList(kConfigsKey);
  if (!configs_list)
    return nullptr;

  for (const auto& config_dict : *configs_list) {
    if (!config_dict.is_dict())
      return nullptr;

    // TODO(oysteine): Remove the per-rule category preset when configs have
    // been updated to just specify the per-config category preset.
    if (!has_global_categories) {
      if (const std::string* category_preset_string =
              config_dict.GetDict().FindString(kConfigCategoryKey)) {
        if (!StringToCategoryPreset(*category_preset_string,
                                    &config->category_preset_)) {
          return nullptr;
        }
      }
    }

    config->AddReactiveRule(config_dict.GetDict());
  }

  if (config->rules().empty())
    return nullptr;

  return config;
}

// static
std::unique_ptr<BackgroundTracingConfigImpl>
BackgroundTracingConfigImpl::SystemFromDict(const base::Value::Dict& dict) {
  auto config = std::make_unique<BackgroundTracingConfigImpl>(
      BackgroundTracingConfigImpl::SYSTEM);

  const base::Value::List* configs_list = dict.FindList(kConfigsKey);
  if (!configs_list)
    return nullptr;

  for (const auto& config_dict : *configs_list) {
    if (!config_dict.is_dict())
      return nullptr;

    config->AddSystemRule(config_dict.GetDict());
  }

  if (config->rules().empty())
    return nullptr;

  return config;
}

BackgroundTracingRule* BackgroundTracingConfigImpl::AddRule(
    const base::Value::Dict& dict) {
  std::unique_ptr<BackgroundTracingRule> rule =
      BackgroundTracingRule::CreateRuleFromDict(dict);
  if (rule) {
    has_crash_scenario_ = rule->is_crash();
    rules_.push_back(std::move(rule));
    return rules_.back().get();
  }
  return nullptr;
}

void BackgroundTracingConfigImpl::SetBufferSizeLimits(
    const base::Value::Dict* dict) {
  if (auto low_ram_buffer_size_kb = dict->FindInt(kConfigLowRamBufferSizeKb)) {
    low_ram_buffer_size_kb_ = *low_ram_buffer_size_kb;
  }
  if (auto medium_ram_buffer_size_kb =
          dict->FindInt(kConfigMediumRamBufferSizeKb)) {
    medium_ram_buffer_size_kb_ = *medium_ram_buffer_size_kb;
  }
  if (auto mobile_network_buffer_size_kb =
          dict->FindInt(kConfigMobileNetworkBuferSizeKb)) {
    mobile_network_buffer_size_kb_ = *mobile_network_buffer_size_kb;
  }
  if (auto max_buffer_size_kb = dict->FindInt(kConfigMaxBufferSizeKb)) {
    max_buffer_size_kb_ = *max_buffer_size_kb;
  }
  if (auto upload_limit_kb = dict->FindInt(kConfigUploadLimitKb)) {
    upload_limit_kb_ = *upload_limit_kb;
  }
  if (auto upload_limit_network_kb =
          dict->FindInt(kConfigUploadLimitNetworkKb)) {
    upload_limit_network_kb_ = *upload_limit_network_kb;
  }
  if (auto interning_reset_interval_ms =
          dict->FindInt(kConfigInterningResetIntervalMs)) {
    interning_reset_interval_ms_ = *interning_reset_interval_ms;
  }
}

int BackgroundTracingConfigImpl::GetMaximumTraceBufferSizeKb() const {
  int64_t ram_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  if (ram_mb > 0 && ram_mb <= 1024) {
    return low_ram_buffer_size_kb_;
  }
#if BUILDFLAG(IS_ANDROID)
  auto type = net::NetworkChangeNotifier::GetConnectionType();
  if (net::NetworkChangeNotifier::IsConnectionCellular(type)) {
    return mobile_network_buffer_size_kb_;
  }
#endif

  if (ram_mb > 0 && ram_mb <= 2 * 1024) {
    return medium_ram_buffer_size_kb_;
  }

  return max_buffer_size_kb_;
}

}  // namespace content
