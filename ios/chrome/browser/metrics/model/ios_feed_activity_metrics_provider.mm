// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_feed_activity_metrics_provider.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"

IOSFeedActivityMetricsProvider::IOSFeedActivityMetricsProvider() = default;

IOSFeedActivityMetricsProvider::~IOSFeedActivityMetricsProvider() = default;

void IOSFeedActivityMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // Log the activity bucket of all loaded BrowserStates.
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    base::UmaHistogramEnumeration(
        kAllFeedsActivityBucketsByProviderHistogram,
        FeedActivityBucketForPrefs(profile->GetPrefs()));
  }
}
