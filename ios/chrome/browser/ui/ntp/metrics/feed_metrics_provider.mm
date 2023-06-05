// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_provider.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FeedMetricsProvider::FeedMetricsProvider() {}

FeedMetricsProvider::~FeedMetricsProvider() {}

void FeedMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // Retrieve activity bucket from storage.
  FeedActivityBucket activityBucket = (FeedActivityBucket)
      [[NSUserDefaults standardUserDefaults] integerForKey:kActivityBucketKey];
  base::UmaHistogramEnumeration(kAllFeedsActivityBucketsByProviderHistogram,
                                activityBucket);
}
