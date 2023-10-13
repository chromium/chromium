// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_feed_activity_metrics_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/metrics/constants.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Tests metrics that are recorded and uploaded by
// IOSFeedActivityMetricsProvider.
class IOSFeedActivityMetricsProviderTest : public PlatformTest {
 public:
  IOSFeedActivityMetricsProviderTest() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    RegisterBrowserStatePrefs(test_pref_service_.registry());
  }

 protected:
  void TearDown() override { PlatformTest::TearDown(); }
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
};

// Tests the implementation of ProvideCurrentSessionData
TEST_F(IOSFeedActivityMetricsProviderTest, ProvideCurrentSessionData) {
  IOSFeedActivityMetricsProvider provider(&test_pref_service_);
  test_pref_service_.SetInteger(kActivityBucketKey, 1);
  provider.ProvideCurrentSessionData(nullptr /* uma_proto */);
  histogram_tester_->ExpectBucketCount(
      kAllFeedsActivityBucketsByProviderHistogram, 1, 1);
}
