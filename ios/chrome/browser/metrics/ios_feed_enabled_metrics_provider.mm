// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_feed_enabled_metrics_provider.h"

#import "base/metrics/histogram_functions.h"
#import "components/feed/core/shared_prefs/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSFeedEnabledMetricsProvider::IOSFeedEnabledMetricsProvider(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

IOSFeedEnabledMetricsProvider::~IOSFeedEnabledMetricsProvider() {}

void IOSFeedEnabledMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  BOOL policy_allows_feed =
      pref_service_->GetBoolean(prefs::kNTPContentSuggestionsEnabled);
  BOOL can_feed_be_shown =
      policy_allows_feed && !IsFeedAblationEnabled() &&
      pref_service_->GetBoolean(prefs::kArticlesForYouEnabled) &&
      pref_service_->GetBoolean(feed::prefs::kArticlesListVisible);
  base::UmaHistogramBoolean("ContentSuggestions.Feed.CanBeShown",
                            can_feed_be_shown);
}
