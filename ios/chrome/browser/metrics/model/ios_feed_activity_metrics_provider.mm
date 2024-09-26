// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_feed_activity_metrics_provider.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"

namespace {

// Returns the activity bucket from `profile`.
FeedActivityBucket FeedActivityBucketForBrowserState(ProfileIOS* profile) {
  const int activity_bucket =
      profile->GetPrefs()->GetInteger(kActivityBucketKey);
  switch (activity_bucket) {
    case base::to_underlying(FeedActivityBucket::kNoActivity):
    case base::to_underlying(FeedActivityBucket::kLowActivity):
    case base::to_underlying(FeedActivityBucket::kMediumActivity):
    case base::to_underlying(FeedActivityBucket::kHighActivity):
      return static_cast<FeedActivityBucket>(activity_bucket);

    default:
      // Do not fail in case of invalid value (to avoid crashing if invalid
      // data is read from disk) but return a value in range.
      DLOG(ERROR) << "Invalid activity bucket value: " << activity_bucket;
      return FeedActivityBucket::kNoActivity;
  }
}

}  // namespace

IOSFeedActivityMetricsProvider::IOSFeedActivityMetricsProvider() = default;

IOSFeedActivityMetricsProvider::~IOSFeedActivityMetricsProvider() = default;

void IOSFeedActivityMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // Log the activity bucket of all loaded BrowserStates.
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    base::UmaHistogramEnumeration(kAllFeedsActivityBucketsByProviderHistogram,
                                  FeedActivityBucketForBrowserState(profile));
  }
}
