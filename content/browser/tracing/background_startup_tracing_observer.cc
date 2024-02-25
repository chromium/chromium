// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_startup_tracing_observer.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/tracing/common/trace_startup_config.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace {

const char kStartupTracingRuleId[] = "org.chromium.background_tracing.startup";

class PreferenceManagerImpl
    : public BackgroundStartupTracingObserver::PreferenceManager {
 public:
  void SetBackgroundStartupTracingEnabled(bool enabled) override {
    tracing::TraceStartupConfig::GetInstance()
        ->SetBackgroundStartupTracingEnabled(enabled);
  }

  bool GetBackgroundStartupTracingEnabled() const override {
    return tracing::TraceStartupConfig::GetInstance()->IsEnabled() &&
           tracing::TraceStartupConfig::GetInstance()->GetSessionOwner() ==
               tracing::TraceStartupConfig::SessionOwner::kBackgroundTracing;
  }
};

}  // namespace

// static
BackgroundStartupTracingObserver&
BackgroundStartupTracingObserver::GetInstance() {
  static base::NoDestructor<BackgroundStartupTracingObserver> instance;
  return *instance;
}

// static
const BackgroundTracingRule*
BackgroundStartupTracingObserver::FindStartupRuleInConfig(
    const BackgroundTracingConfigImpl& config) {
  for (const auto& rule : config.rules()) {
    if (rule->rule_id() == kStartupTracingRuleId) {
      return rule.get();
    }
  }
  return nullptr;
}

BackgroundStartupTracingObserver::BackgroundStartupTracingObserver()
    : enabled_in_current_session_(false),
      preferences_(new PreferenceManagerImpl) {}

BackgroundStartupTracingObserver::~BackgroundStartupTracingObserver() {}

void BackgroundStartupTracingObserver::SetPreferenceManagerForTesting(
    std::unique_ptr<PreferenceManager> preferences) {
  preferences_ = std::move(preferences);
}

std::unique_ptr<BackgroundTracingConfigImpl>
BackgroundStartupTracingObserver::IncludeStartupConfigIfNeeded(
    std::unique_ptr<BackgroundTracingConfigImpl> config) {
  enabled_in_current_session_ =
      preferences_->GetBackgroundStartupTracingEnabled();

  const BackgroundTracingRule* startup_rule = nullptr;
  if (config) {
    startup_rule = FindStartupRuleInConfig(*config);
  }

  // Reset the flag if startup tracing was enabled again in current session.
  if (startup_rule) {
    preferences_->SetBackgroundStartupTracingEnabled(true);
  } else {
    preferences_->SetBackgroundStartupTracingEnabled(false);
  }

  // If we're preemptive tracing then OnScenarioActivated() would just
  // immediately finalize tracing, rather than starting it.
  if (config &&
      (config->tracing_mode() == BackgroundTracingConfigImpl::PREEMPTIVE)) {
    enabled_in_current_session_ = false;
    return config;
  }

  // If enabled in current session and startup rule already exists, then do not
  // add another rule.
  if (!enabled_in_current_session_ || startup_rule)
    return config;

  auto rules_dict = base::Value::Dict()
                        .Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED")
                        .Set("trigger_name", kStartupTracingTriggerName)
                        .Set("trigger_delay", 30)
                        .Set("rule_id", kStartupTracingRuleId);

  if (config) {
    config->AddReactiveRule(rules_dict);
  } else {
    config =
        BackgroundTracingConfigImpl::ReactiveFromDict(base::Value::Dict().Set(
            "configs", base::Value::List().Append(std::move(rules_dict))));
  }
  DCHECK(FindStartupRuleInConfig(*config));
  return config;
}

}  // namespace content
