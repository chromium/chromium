// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_REACHED_CODE_TRACING_OBSERVER_ANDROID_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_REACHED_CODE_TRACING_OBSERVER_ANDROID_H_

#include <memory>

#include "content/browser/tracing/background_tracing_manager_impl.h"

namespace content {

// Observes for reached code tracing config and sets up background tracing
// scenario to profile reached code.
class CONTENT_EXPORT BackgroundReachedCodeTracingObserver
    : public BackgroundTracingManagerImpl::EnabledStateObserver {
 public:
  static BackgroundReachedCodeTracingObserver& GetInstance();

  static void ResetForTesting();

  // BackgroundTracingManagerImpl::EnabledStateObserver implementation.
  void OnScenarioActivated(const BackgroundTracingConfigImpl* config) override;
  void OnScenarioAborted() override;
  void OnTracingEnabled(
      BackgroundTracingConfigImpl::CategoryPreset preset) override;

  std::unique_ptr<BackgroundTracingConfigImpl> IncludeReachedCodeConfigIfNeeded(
      std::unique_ptr<BackgroundTracingConfigImpl> config);

  bool enabled_in_current_session() const {
    return enabled_in_current_session_;
  }

  BackgroundReachedCodeTracingObserver(BackgroundReachedCodeTracingObserver&&) =
      delete;
  BackgroundReachedCodeTracingObserver& operator=(
      BackgroundReachedCodeTracingObserver&&) = delete;

 private:
  friend class base::NoDestructor<BackgroundReachedCodeTracingObserver>;

  BackgroundReachedCodeTracingObserver();
  ~BackgroundReachedCodeTracingObserver() override;

  bool enabled_in_current_session_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_REACHED_CODE_TRACING_OBSERVER_ANDROID_H_
