// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_STARTUP_TRACING_OBSERVER_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_STARTUP_TRACING_OBSERVER_H_

#include <memory>

#include "base/no_destructor.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/common/content_export.h"

namespace content {

// Observes for startup tracing config and sets up preferences to trace on next
// startup.
class CONTENT_EXPORT BackgroundStartupTracingObserver
    : public BackgroundTracingManagerImpl::EnabledStateObserver {
 public:
  // Delegate to store and read application preferences for startup tracing, to
  // isolate the feature for testing.
  class PreferenceManager {
   public:
    virtual void SetBackgroundStartupTracingEnabled(bool enabled) = 0;
    virtual bool GetBackgroundStartupTracingEnabled() const = 0;
    virtual ~PreferenceManager() = default;
  };

  static BackgroundStartupTracingObserver& GetInstance();

  BackgroundStartupTracingObserver(const BackgroundStartupTracingObserver&) =
      delete;
  BackgroundStartupTracingObserver& operator=(
      const BackgroundStartupTracingObserver&) = delete;

  // Returns startup tracing rule from the given config, null if doesn't exist.
  static const BackgroundTracingRule* FindStartupRuleInConfig(
      const BackgroundTracingConfigImpl& config);

  // BackgroundTracingManagerImpl::EnabledStateObserver implementation.
  void OnScenarioActivated(const BackgroundTracingConfigImpl* config) override;
  void OnScenarioAborted() override;
  void OnTracingEnabled() override;

  // Reads the preference for startup tracing set on the previous startup and
  // includes config for startup tracing if enabled. Also sets or resets the
  // preference for next session based on current config containing startup
  // rule. Returns the config after the changes.
  std::unique_ptr<BackgroundTracingConfigImpl> IncludeStartupConfigIfNeeded(
      std::unique_ptr<BackgroundTracingConfigImpl> config);

  // Returns true if startup tracing was set on the previous session and is
  // active now.
  bool enabled_in_current_session() const {
    return enabled_in_current_session_;
  }

  void SetPreferenceManagerForTesting(
      std::unique_ptr<PreferenceManager> preferences);

 private:
  BackgroundStartupTracingObserver();
  ~BackgroundStartupTracingObserver() override;

  bool enabled_in_current_session_;

  std::unique_ptr<PreferenceManager> preferences_;

  // For `BackgroundStartupTracingObserver::GetInstance`.
  friend class base::NoDestructor<BackgroundStartupTracingObserver>;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_STARTUP_TRACING_OBSERVER_H_
