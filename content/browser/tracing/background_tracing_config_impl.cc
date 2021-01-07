// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_tracing_config_impl.h"

#include <set>
#include <utility>

#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_handle.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/tracing/common/trace_startup_config.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "net/base/network_change_notifier.h"

using base::trace_event::TraceConfig;

namespace content {

namespace {

const char kConfigsKey[] = "configs";

const char kConfigModeKey[] = "mode";
const char kConfigModePreemptive[] = "PREEMPTIVE_TRACING_MODE";
const char kConfigModeReactive[] = "REACTIVE_TRACING_MODE";
const char kConfigModeSystem[] = "SYSTEM_TRACING_MODE";

const char kConfigScenarioName[] = "scenario_name";
const char kConfigTraceBrowserProcessOnly[] = "trace_browser_process_only";
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

BackgroundTracingConfigImpl::~BackgroundTracingConfigImpl() {}

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
      NOTREACHED();
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

void BackgroundTracingConfigImpl::IntoDict(base::DictionaryValue* dict) {
  if (category_preset_ == CUSTOM_CATEGORY_PRESET) {
    dict->SetString(kConfigCustomCategoriesKey, custom_categories_);
  } else if (category_preset_ == CUSTOM_TRACE_CONFIG) {
    base::Optional<base::Value> trace_config =
        base::JSONReader::Read(trace_config_.ToString());
    if (trace_config) {
      dict->SetKey(kConfigTraceConfigKey, std::move(*trace_config));
    }
  }
  if (!enabled_data_sources_.empty()) {
    dict->SetString(kEnabledDataSourcesKey, enabled_data_sources_);
  }

  switch (tracing_mode()) {
    case BackgroundTracingConfigImpl::PREEMPTIVE:
      dict->SetString(kConfigModeKey, kConfigModePreemptive);
      dict->SetString(kConfigCategoryKey,
                      CategoryPresetToString(category_preset_));
      break;
    case BackgroundTracingConfigImpl::REACTIVE:
      dict->SetString(kConfigModeKey, kConfigModeReactive);
      break;
    case BackgroundTracingConfigImpl::SYSTEM:
      dict->SetString(kConfigModeKey, kConfigModeSystem);
      break;
  }

  std::unique_ptr<base::ListValue> configs_list(new base::ListValue());
  for (const auto& rule : rules_) {
    std::unique_ptr<base::DictionaryValue> config_dict(
        new base::DictionaryValue());
    DCHECK(rule);
    rule->IntoDict(config_dict.get());
    configs_list->Append(std::move(config_dict));
  }

  dict->Set(kConfigsKey, std::move(configs_list));

  if (!scenario_name_.empty())
    dict->SetString(kConfigScenarioName, scenario_name_);
}

void BackgroundTracingConfigImpl::AddPreemptiveRule(
    const base::DictionaryValue* dict) {
  AddRule(dict);
}

void BackgroundTracingConfigImpl::AddReactiveRule(
    const base::DictionaryValue* dict,
    BackgroundTracingConfigImpl::CategoryPreset category_preset) {
  BackgroundTracingRule* rule = AddRule(dict);
  if (rule) {
    rule->set_category_preset(category_preset);
  }
}

void BackgroundTracingConfigImpl::AddSystemRule(
    const base::DictionaryValue* dict) {
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
  } else {
    chrome_config = GetConfigForCategoryPreset(category_preset(), record_mode);
  }

  if (trace_browser_process_only_) {
    TraceConfig::ProcessFilterConfig process_config({base::GetCurrentProcId()});
    chrome_config.SetProcessFilterConfig(process_config);
  }

  chrome_config.SetTraceBufferSizeInKb(GetMaximumTraceBufferSizeKb());

#if defined(OS_ANDROID)
  // For legacy tracing backend, set low trace buffer size on Android in order
  // to upload small trace files.
  if (tracing_mode() == BackgroundTracingConfigImpl::PREEMPTIVE) {
    chrome_config.SetTraceBufferSizeInEvents(20000);
  }
#endif

  return chrome_config;
}

size_t BackgroundTracingConfigImpl::GetTraceUploadLimitKb() const {
#if defined(OS_ANDROID)
  auto type = net::NetworkChangeNotifier::GetConnectionType();
  UMA_HISTOGRAM_ENUMERATION(
      "Tracing.Background.NetworkConnectionTypeWhenUploaded", type,
      net::NetworkChangeNotifier::CONNECTION_LAST + 1);
  if (net::NetworkChangeNotifier::IsConnectionCellular(type)) {
    return upload_limit_network_kb_;
  }
#endif
  return upload_limit_kb_;
}

// static
std::unique_ptr<BackgroundTracingConfigImpl>
BackgroundTracingConfigImpl::FromDict(const base::DictionaryValue* dict) {
  DCHECK(dict);

  std::string mode;
  if (!dict->GetString(kConfigModeKey, &mode))
    return nullptr;

  std::unique_ptr<BackgroundTracingConfigImpl> config;

  if (mode == kConfigModePreemptive) {
    config = PreemptiveFromDict(dict);
  } else if (mode == kConfigModeReactive) {
    config = ReactiveFromDict(dict);
  } else if (mode == kConfigModeSystem) {
    config = SystemFromDict(dict);
  } else {
    return nullptr;
  }

  if (config) {
    dict->GetString(kConfigScenarioName, &config->scenario_name_);
    config->SetBufferSizeLimits(dict);
    bool value = false;
    if (dict->GetBoolean(kConfigTraceBrowserProcessOnly, &value)) {
      config->trace_browser_process_only_ = value;
    }
  }

  return config;
}

// static
std::unique_ptr<BackgroundTracingConfigImpl>
BackgroundTracingConfigImpl::PreemptiveFromDict(
    const base::DictionaryValue* dict) {
  DCHECK(dict);

  std::unique_ptr<BackgroundTracingConfigImpl> config(
      new BackgroundTracingConfigImpl(BackgroundTracingConfigImpl::PREEMPTIVE));

  const base::DictionaryValue* trace_config = nullptr;
  if (dict->GetDictionary(kConfigTraceConfigKey, &trace_config)) {
    config->trace_config_ = TraceConfig(*trace_config);
    config->category_preset_ = CUSTOM_TRACE_CONFIG;
  } else if (dict->GetString(kConfigCustomCategoriesKey,
                             &config->custom_categories_)) {
    config->category_preset_ = CUSTOM_CATEGORY_PRESET;
  } else {
    std::string category_preset_string;
    if (!dict->GetString(kConfigCategoryKey, &category_preset_string))
      return nullptr;

    if (!StringToCategoryPreset(category_preset_string,
                                &config->category_preset_)) {
      return nullptr;
    }
  }
  if (const std::string* enabled_data_sources =
          dict->FindStringKey(kEnabledDataSourcesKey)) {
    config->enabled_data_sources_ = *enabled_data_sources;
  }

  const base::ListValue* configs_list = nullptr;
  if (!dict->GetList(kConfigsKey, &configs_list))
    return nullptr;

  for (const auto& it : *configs_list) {
    const base::DictionaryValue* config_dict = nullptr;
    if (!it.GetAsDictionary(&config_dict))
      return nullptr;

    config->AddPreemptiveRule(config_dict);
  }

  if (config->rules().empty())
    return nullptr;

  return config;
}

// static
std::unique_ptr<BackgroundTracingConfigImpl>
BackgroundTracingConfigImpl::ReactiveFromDict(
    const base::DictionaryValue* dict) {
  DCHECK(dict);

  std::unique_ptr<BackgroundTracingConfigImpl> config(
      new BackgroundTracingConfigImpl(BackgroundTracingConfigImpl::REACTIVE));

  std::string category_preset_string;
  bool has_global_categories = false;
  const base::DictionaryValue* trace_config = nullptr;
  if (dict->GetDictionary(kConfigTraceConfigKey, &trace_config)) {
    config->trace_config_ = TraceConfig(*trace_config);
    config->category_preset_ = CUSTOM_TRACE_CONFIG;
    has_global_categories = true;
  } else if (dict->GetString(kConfigCustomCategoriesKey,
                             &config->custom_categories_)) {
    config->category_preset_ = CUSTOM_CATEGORY_PRESET;
    has_global_categories = true;
  } else if (dict->GetString(kConfigCategoryKey, &category_preset_string)) {
    if (!StringToCategoryPreset(category_preset_string,
                                &config->category_preset_)) {
      return nullptr;
    }
    has_global_categories = true;
  }

  if (const std::string* enabled_data_sources =
          dict->FindStringKey(kEnabledDataSourcesKey)) {
    config->enabled_data_sources_ = *enabled_data_sources;
  }

  const base::ListValue* configs_list = nullptr;
  if (!dict->GetList(kConfigsKey, &configs_list))
    return nullptr;

  for (const auto& it : *configs_list) {
    const base::DictionaryValue* config_dict = nullptr;
    if (!it.GetAsDictionary(&config_dict))
      return nullptr;

    // TODO(oysteine): Remove the per-rule category preset when configs have
    // been updated to just specify the per-config category preset.
    if (!has_global_categories &&
        config_dict->GetString(kConfigCategoryKey, &category_preset_string)) {
      if (!StringToCategoryPreset(category_preset_string,
                                  &config->category_preset_)) {
        return nullptr;
      }
    }

    config->AddReactiveRule(config_dict, config->category_preset_);
  }

  if (config->rules().empty())
    return nullptr;

  return config;
}

// static
std::unique_ptr<BackgroundTracingConfigImpl>
BackgroundTracingConfigImpl::SystemFromDict(const base::DictionaryValue* dict) {
  DCHECK(dict);

  auto config = std::make_unique<BackgroundTracingConfigImpl>(
      BackgroundTracingConfigImpl::SYSTEM);

  const base::ListValue* configs_list = nullptr;
  if (!dict->GetList(kConfigsKey, &configs_list))
    return nullptr;

  for (const auto& it : *configs_list) {
    const base::DictionaryValue* config_dict = nullptr;
    if (!it.GetAsDictionary(&config_dict))
      return nullptr;

    config->AddSystemRule(config_dict);
  }

  if (config->rules().empty())
    return nullptr;

  return config;
}

// static
TraceConfig BackgroundTracingConfigImpl::GetConfigForCategoryPreset(
    BackgroundTracingConfigImpl::CategoryPreset preset,
    base::trace_event::TraceRecordMode record_mode) {
  switch (preset) {
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_STARTUP: {
      auto config =
          tracing::TraceStartupConfig::GetDefaultBrowserStartupConfig();
      config.SetTraceRecordMode(record_mode);
      return config;
    }
    default:
      NOTREACHED();
      return TraceConfig();
  }
}

BackgroundTracingRule* BackgroundTracingConfigImpl::AddRule(
    const base::DictionaryValue* dict) {
  std::unique_ptr<BackgroundTracingRule> rule =
      BackgroundTracingRule::CreateRuleFromDict(dict);
  if (rule) {
    rules_.push_back(std::move(rule));
    return rules_.back().get();
  }
  return nullptr;
}

void BackgroundTracingConfigImpl::SetBufferSizeLimits(
    const base::DictionaryValue* dict) {
  int value = 0;
  if (dict->GetInteger(kConfigLowRamBufferSizeKb, &value)) {
    low_ram_buffer_size_kb_ = value;
  }
  if (dict->GetInteger(kConfigMediumRamBufferSizeKb, &value)) {
    medium_ram_buffer_size_kb_ = value;
  }
  if (dict->GetInteger(kConfigMobileNetworkBuferSizeKb, &value)) {
    mobile_network_buffer_size_kb_ = value;
  }
  if (dict->GetInteger(kConfigMaxBufferSizeKb, &value)) {
    max_buffer_size_kb_ = value;
  }
  if (dict->GetInteger(kConfigUploadLimitKb, &value)) {
    upload_limit_kb_ = value;
  }
  if (dict->GetInteger(kConfigUploadLimitNetworkKb, &value)) {
    upload_limit_network_kb_ = value;
  }
  if (dict->GetInteger(kConfigInterningResetIntervalMs, &value)) {
    interning_reset_interval_ms_ = value;
  }
}

int BackgroundTracingConfigImpl::GetMaximumTraceBufferSizeKb() const {
  int64_t ram_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  if (ram_mb > 0 && ram_mb <= 1024) {
    return low_ram_buffer_size_kb_;
  }
#if defined(OS_ANDROID)
  auto type = net::NetworkChangeNotifier::GetConnectionType();
  UMA_HISTOGRAM_ENUMERATION(
      "Tracing.Background.NetworkConnectionTypeWhenStarted", type,
      net::NetworkChangeNotifier::CONNECTION_LAST + 1);
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
