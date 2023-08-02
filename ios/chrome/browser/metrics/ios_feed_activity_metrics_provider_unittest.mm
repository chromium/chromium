// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_feed_activity_metrics_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Tests metrics that are recorded and uploaded by
// IOSFeedActivityMetricsProvider.
class IOSFeedActivityMetricsProviderTest : public PlatformTest {
 public:
  IOSFeedActivityMetricsProviderTest() {
    histogram_tester_.reset(new base::HistogramTester());
  }

 protected:
  void TearDown() override { PlatformTest::TearDown(); }
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests the implementation of ProvideCurrentSessionData
TEST_F(IOSFeedActivityMetricsProviderTest, ProvideCurrentSessionData) {
  IOSFeedActivityMetricsProvider provider;
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setInteger:1 forKey:kActivityBucketKey];
  provider.ProvideCurrentSessionData(nullptr /* uma_proto */);
  histogram_tester_->ExpectBucketCount(
      kAllFeedsActivityBucketsByProviderHistogram, 1, 1);
}
