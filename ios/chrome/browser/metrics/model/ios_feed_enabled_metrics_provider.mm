// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_feed_enabled_metrics_provider.h"

#import "base/metrics/histogram_functions.h"
#import "components/feed/core/shared_prefs/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

// Returns whether the Feed can be displayed according for `prefs`.
bool CanDisplayFeed(PrefService* prefs) {
  BOOL is_feed_enabled_by_user =
      prefs->GetBoolean(prefs::kArticlesForYouEnabled) &&
      (IsHomeCustomizationEnabled() ||
       prefs->GetBoolean(feed::prefs::kArticlesListVisible));
  return is_feed_enabled_by_user &&
         prefs->GetBoolean(prefs::kNTPContentSuggestionsEnabled) &&
         !IsFeedAblationEnabled();
}

}  // namespace

IOSFeedEnabledMetricsProvider::IOSFeedEnabledMetricsProvider() = default;

IOSFeedEnabledMetricsProvider::~IOSFeedEnabledMetricsProvider() = default;

void IOSFeedEnabledMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // Log whether the Feed can be displayed for each loaded profile.
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    base::UmaHistogramBoolean(kFeedEnabledHistogram,
                              CanDisplayFeed(profile->GetPrefs()));
  }
}
