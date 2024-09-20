// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/model/translate_ranker_metrics_provider.h"

#import "components/translate/core/browser/translate_ranker.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/translate/model/translate_ranker_factory.h"
#import "ios/web/public/browser_state.h"
#import "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#import "third_party/metrics_proto/translate_event.pb.h"

namespace translate {

void TranslateRankerMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    TranslateRanker* ranker = TranslateRankerFactory::GetForProfile(profile);
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
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    TranslateRanker* ranker = TranslateRankerFactory::GetForProfile(profile);
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
