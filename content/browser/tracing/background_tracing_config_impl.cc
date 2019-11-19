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

const char kConfigCategoryKey[] = "category";
const char kConfigCustomCategoriesKey[] = "custom_categories";
const char kConfigTraceConfigKey[] = "trace_config";
const char kConfigCategoryBenchmark[] = "BENCHMARK";
const char kConfigCategoryBenchmarkDeep[] = "BENCHMARK_DEEP";
const char kConfigCategoryBenchmarkGPU[] = "BENCHMARK_GPU";
const char kConfigCategoryBenchmarkIPC[] = "BENCHMARK_IPC";
const char kConfigCategoryBenchmarkStartup[] = "BENCHMARK_STARTUP";
const char kConfigCategoryBenchmarkBlinkGC[] = "BENCHMARK_BLINK_GC";
const char kConfigCategoryBenchmarkMemoryHeavy[] = "BENCHMARK_MEMORY_HEAVY";
const char kConfigCategoryBenchmarkMemoryLight[] = "BENCHMARK_MEMORY_LIGHT";
const char kConfigCategoryBenchmarkExecutionMetric[] =
    "BENCHMARK_EXECUTION_METRIC";
const char kConfigCategoryBenchmarkNavigation[] = "BENCHMARK_NAVIGATION";
const char kConfigCategoryBenchmarkRenderers[] = "BENCHMARK_RENDERERS";
const char kConfigCategoryBenchmarkServiceworker[] = "BENCHMARK_SERVICEWORKER";
const char kConfigCategoryBenchmarkPower[] = "BENCHMARK_POWER";
const char kConfigCategoryBlinkStyle[] = "BLINK_STYLE";
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
      category_preset_(BackgroundTracingConfigImpl::BENCHMARK) {}

BackgroundTracingConfigImpl::~BackgroundTracingConfigImpl() {}

// static
std::string BackgroundTracingConfigImpl::CategoryPresetToString(
    BackgroundTracingConfigImpl::CategoryPreset category_preset) {
  switch (category_preset) {
    case BackgroundTracingConfigImpl::BENCHMARK:
      return kConfigCategoryBenchmark;
    case BackgroundTracingConfigImpl::BENCHMARK_DEEP:
      return kConfigCategoryBenchmarkDeep;
    case BackgroundTracingConfigImpl::BENCHMARK_GPU:
      return kConfigCategoryBenchmarkGPU;
    case BackgroundTracingConfigImpl::BENCHMARK_IPC:
      return kConfigCategoryBenchmarkIPC;
    case BackgroundTracingConfigImpl::BENCHMARK_STARTUP:
      return kConfigCategoryBenchmarkStartup;
    case BackgroundTracingConfigImpl::BENCHMARK_BLINK_GC:
      return kConfigCategoryBenchmarkBlinkGC;
    case BackgroundTracingConfigImpl::BENCHMARK_MEMORY_HEAVY:
      return kConfigCategoryBenchmarkMemoryHeavy;
    case BackgroundTracingConfigImpl::BENCHMARK_MEMORY_LIGHT:
      return kConfigCategoryBenchmarkMemoryLight;
    case BackgroundTracingConfigImpl::BENCHMARK_EXECUTION_METRIC:
      return kConfigCategoryBenchmarkExecutionMetric;
    case BackgroundTracingConfigImpl::BENCHMARK_NAVIGATION:
      return kConfigCategoryBenchmarkNavigation;
    case BackgroundTracingConfigImpl::BENCHMARK_RENDERERS:
      return kConfigCategoryBenchmarkRenderers;
    case BackgroundTracingConfigImpl::BENCHMARK_SERVICEWORKER:
      return kConfigCategoryBenchmarkServiceworker;
    case BackgroundTracingConfigImpl::BENCHMARK_POWER:
      return kConfigCategoryBenchmarkPower;
    case BackgroundTracingConfigImpl::BLINK_STYLE:
      return kConfigCategoryBlinkStyle;
    case BackgroundTracingConfigImpl::CUSTOM_CATEGORY_PRESET:
      return kConfigCategoryCustom;
    case BackgroundTracingConfigImpl::CUSTOM_TRACE_CONFIG:
      return kConfigCustomConfig;
    case BackgroundTracingConfigImpl::CATEGORY_PRESET_UNSET:
      NOTREACHED();
  }
  NOTREACHED();
  return "";
}

// static
bool BackgroundTracingConfigImpl::StringToCategoryPreset(
    const std::string& category_preset_string,
    BackgroundTracingConfigImpl::CategoryPreset* category_preset) {
  if (category_preset_string == kConfigCategoryBenchmark) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK;
    return true;
  }

  if (category_preset_string == kConfigCategoryBenchmarkDeep) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_DEEP;
    return true;
  }

  if (category_preset_string == kConfigCategoryBenchmarkGPU) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_GPU;
    return true;
  }

  if (category_preset_string == kConfigCategoryBenchmarkIPC) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_IPC;
    return true;
  }

  if (category_preset_string == kConfigCategoryBenchmarkStartup) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_STARTUP;
    return true;
  }

  if (category_preset_string == kConfigCategoryBenchmarkBlinkGC) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_BLINK_GC;
    return true;
  }

  if (category_preset_string == kConfigCategoryBenchmarkMemoryHeavy) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_MEMORY_HEAVY;
    return true;
  }

  if (category_preset_string == kConfigCategoryBenchmarkMemoryLight) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_MEMORY_LIGHT;
    return true;
  }

  if (category_preset_string == kConfigCategoryBenchmarkExecutionMetric) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_EXECUTION_METRIC;
    return true;
  }

  if (category_preset_string == kConfigCategoryBenchmarkNavigation) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_NAVIGATION;
    return true;
  }

  if (category_preset_string == kConfigCategoryBenchmarkRenderers) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_RENDERERS;
    return true;
  }

  if (category_preset_string == kConfigCategoryBenchmarkServiceworker) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_SERVICEWORKER;
    return true;
  }

  if (category_preset_string == kConfigCategoryBenchmarkPower) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_POWER;
    return true;
  }

  if (category_preset_string == kConfigCategoryBlinkStyle) {
    *category_preset = BackgroundTracingConfigImpl::BLINK_STYLE;
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
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK:
      return TraceConfig("benchmark,toplevel", record_mode);
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_DEEP:
      return TraceConfig(
          "*,disabled-by-default-benchmark.detailed,"
          "disabled-by-default-v8.cpu_profile,"
          "disabled-by-default-v8.runtime_stats",
          record_mode);
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_GPU:
      return TraceConfig(
          "benchmark,toplevel,gpu,base,mojom,ipc,"
          "disabled-by-default-system_stats,disabled-by-default-cpu_profiler",
          record_mode);
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_IPC:
      return TraceConfig("benchmark,toplevel,ipc", record_mode);
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_STARTUP: {
      auto config =
          tracing::TraceStartupConfig::GetDefaultBrowserStartupConfig();
      config.SetTraceRecordMode(record_mode);
      return config;
    }
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_BLINK_GC:
      return TraceConfig("blink_gc,disabled-by-default-blink_gc", record_mode);
    case BackgroundTracingConfigImpl::CategoryPreset::
        BENCHMARK_EXECUTION_METRIC:
      return TraceConfig("blink.console,v8", record_mode);
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_NAVIGATION: {
      auto config = TraceConfig(
          "benchmark,toplevel,ipc,base,browser,navigation,omnibox,ui,shutdown,"
          "safe_browsing,Java,EarlyJava,loading,startup,mojom,renderer_host,"
          "disabled-by-default-system_stats,disabled-by-default-cpu_profiler,"
          "dwrite,fonts,ServiceWorker",
          record_mode);
      // Filter only browser process events.
      base::trace_event::TraceConfig::ProcessFilterConfig process_config(
          {base::GetCurrentProcId()});
      config.SetProcessFilterConfig(process_config);
      return config;
    }
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_RENDERERS:
      return TraceConfig(
          "benchmark,toplevel,ipc,base,ui,v8,renderer,blink,blink_gc,mojom,"
          "latency,latencyInfo,renderer_host,cc,memory,dwrite,fonts,browser,"
          "ServiceWorker,disabled-by-default-v8.gc,"
          "disabled-by-default-blink_gc,disabled-by-default-lifecycles,"
          "disabled-by-default-renderer.scheduler,"
          "disabled-by-default-system_stats,disabled-by-default-cpu_profiler",
          record_mode);
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_SERVICEWORKER:
      return TraceConfig(
          "benchmark,toplevel,ipc,base,ServiceWorker,CacheStorage,Blob,"
          "IndexedDB,loading,mojom,navigation,renderer,blink,blink_gc,blink."
          "user_timing,blink.worker,fonts,startup,disabled-by-default-cpu_"
          "profiler,disabled-by-default-network",
          record_mode);
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_POWER:
      return TraceConfig(
          "benchmark,toplevel,ipc,base,audio,compositor,gpu,media,memory,midi,"
          "native,omnibox,renderer,skia,task_scheduler,ui,v8,views,webaudio,"
          "disabled-by-default-cpu_profiler",
          record_mode);
    case BackgroundTracingConfigImpl::CategoryPreset::BLINK_STYLE:
      return TraceConfig("blink_style", record_mode);

    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_MEMORY_HEAVY:
      return TraceConfig("-*,disabled-by-default-memory-infra", record_mode);
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_MEMORY_LIGHT: {
      // On memory light mode, the periodic memory dumps are disabled.
      base::trace_event::TraceConfig::MemoryDumpConfig memory_config;
      memory_config.allowed_dump_modes =
          std::set<base::trace_event::MemoryDumpLevelOfDetail>(
              {base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND});
      TraceConfig config("-*,disabled-by-default-memory-infra", record_mode);
      config.ResetMemoryDumpConfig(memory_config);
      return config;
    }
    case BackgroundTracingConfigImpl::CategoryPreset::CATEGORY_PRESET_UNSET:
    case BackgroundTracingConfigImpl::CategoryPreset::CUSTOM_CATEGORY_PRESET:
    case BackgroundTracingConfigImpl::CategoryPreset::CUSTOM_TRACE_CONFIG:
      NOTREACHED();
  }
  NOTREACHED();
  return TraceConfig();
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
