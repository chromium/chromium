// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_feed_activity_metrics_provider.h"

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/metrics/model/constants.h"

IOSFeedActivityMetricsProvider::IOSFeedActivityMetricsProvider(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
}

IOSFeedActivityMetricsProvider::~IOSFeedActivityMetricsProvider() {}

void IOSFeedActivityMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // Retrieve activity bucket from storage.
  int activityBucket = pref_service_->GetInteger(kActivityBucketKey);
  base::UmaHistogramExactLinear(kAllFeedsActivityBucketsByProviderHistogram,
                                activityBucket, 4);
}
