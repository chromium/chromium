// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/translate_ranker_metrics_provider.h"

#include "components/translate/core/browser/translate_ranker.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/translate/translate_ranker_factory.h"
#include "ios/web/public/browser_state.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/translate_event.pb.h"

namespace translate {

void TranslateRankerMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  std::vector<ChromeBrowserState*> browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  for (auto* state : browser_states) {
    TranslateRanker* ranker = TranslateRankerFactory::GetForBrowserState(state);
    DCHECK(ranker);
    UpdateLoggingState();
    std::vector<metrics::TranslateEventProto> translate_events;
    ranker->FlushTranslateEvents(&translate_events);

    for (auto& event : translate_events) {
      uma_proto->add_translate_event()->Swap(&event);
    }
  }
}

void TranslateRankerMetricsProvider::UpdateLoggingState() {
  std::vector<ChromeBrowserState*> browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  for (auto* state : browser_states) {
    TranslateRanker* ranker = TranslateRankerFactory::GetForBrowserState(state);
    DCHECK(ranker);
    ranker->EnableLogging(logging_enabled_);
  }
}

void TranslateRankerMetricsProvider::OnRecordingEnabled() {
  logging_enabled_ = true;
  UpdateLoggingState();
}

void TranslateRankerMetricsProvider::OnRecordingDisabled() {
  logging_enabled_ = false;
  UpdateLoggingState();
}

}  // namespace translate
