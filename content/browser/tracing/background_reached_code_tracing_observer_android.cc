// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_reached_code_tracing_observer_android.h"

#include "base/android/reached_code_profiler.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/tracing/public/mojom/perfetto_service.mojom-forward.h"

namespace content {

namespace {

const char kReachedCodeTracingConfig[] = "reached-code-config";

BackgroundReachedCodeTracingObserver* g_trace_log_for_testing = nullptr;

}  // namespace

// static
BackgroundReachedCodeTracingObserver&
BackgroundReachedCodeTracingObserver::GetInstance() {
  static base::NoDestructor<BackgroundReachedCodeTracingObserver> instance;
  return *instance;
}

// static
void BackgroundReachedCodeTracingObserver::ResetForTesting() {
  if (!g_trace_log_for_testing)
    return;
  g_trace_log_for_testing->~BackgroundReachedCodeTracingObserver();
  new (g_trace_log_for_testing) BackgroundReachedCodeTracingObserver;
}

BackgroundReachedCodeTracingObserver::BackgroundReachedCodeTracingObserver()
    : enabled_in_current_session_(
          base::android::IsReachedCodeProfilerEnabled()) {
  g_trace_log_for_testing = this;
}

BackgroundReachedCodeTracingObserver::~BackgroundReachedCodeTracingObserver() =
    default;

void BackgroundReachedCodeTracingObserver::OnScenarioActivated(
    const BackgroundTracingConfigImpl* config) {
  if (!enabled_in_current_session_)
    return;
  BackgroundTracingManagerImpl& manager =
      BackgroundTracingManagerImpl::GetInstance();
  BackgroundTracingManager::TriggerHandle handle =
      manager.RegisterTriggerType(kReachedCodeTracingConfig);

  BackgroundTracingManagerImpl::GetInstance().TriggerNamedEvent(
      handle, BackgroundTracingManager::StartedFinalizingCallback());
}

void BackgroundReachedCodeTracingObserver::OnScenarioAborted() {
  enabled_in_current_session_ = false;
}

void BackgroundReachedCodeTracingObserver::OnTracingEnabled(
    BackgroundTracingConfigImpl::CategoryPreset preset) {}

std::unique_ptr<BackgroundTracingConfigImpl>
BackgroundReachedCodeTracingObserver::IncludeReachedCodeConfigIfNeeded(
    std::unique_ptr<BackgroundTracingConfigImpl> config) {
  if (!enabled_in_current_session_) {
    return config;
  }

  if (config) {
    enabled_in_current_session_ = false;
    return config;
  }

  base::Value rules_dict(base::Value::Type::DICTIONARY);
  rules_dict.SetStringKey("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
  rules_dict.SetStringKey("trigger_name", kReachedCodeTracingConfig);
  rules_dict.SetIntKey("trigger_delay", 30);

  base::Value dict(base::Value::Type::DICTIONARY);
  base::Value rules_list(base::Value::Type::LIST);
  rules_list.Append(std::move(rules_dict));
  dict.SetKey("configs", std::move(rules_list));
  dict.SetStringKey(
      "enabled_data_sources",
      base::StrCat({tracing::mojom::kMetaDataSourceName, ",",
                    tracing::mojom::kReachedCodeProfilerSourceName}));
  dict.SetStringKey("category", "CUSTOM");
  dict.SetStringKey("custom_categories", "-*");

  config = BackgroundTracingConfigImpl::ReactiveFromDict(dict);
  return config;
}

}  // namespace content
