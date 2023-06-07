// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_constants.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests metrics that are recorded and uploaded by
// FeedMetricsProvider.
class FeedMetricsProviderTest : public PlatformTest {
 public:
  FeedMetricsProviderTest() {
    histogram_tester_.reset(new base::HistogramTester());
  }

 protected:
  void TearDown() override { PlatformTest::TearDown(); }
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests the implementation of ProvideCurrentSessionData
TEST_F(FeedMetricsProviderTest, ProvideCurrentSessionData) {
  FeedMetricsProvider provider;
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setInteger:1 forKey:kActivityBucketKey];
  provider.ProvideCurrentSessionData(nullptr /* uma_proto */);
  histogram_tester_->ExpectBucketCount(
      kAllFeedsActivityBucketsByProviderHistogram, 1, 1);
}
