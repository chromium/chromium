// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_push_notifications_metrics_provider.h"

#import "base/metrics/histogram_functions.h"
#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Tests metrics that are recorded and uploaded by
// IOSPushNotificationsMetricsProvider.
class IOSPushNotificationsMetricsProviderTest : public PlatformTest {
 public:
  IOSPushNotificationsMetricsProviderTest() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

 protected:
  void TearDown() override { PlatformTest::TearDown(); }
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(IOSPushNotificationsMetricsProviderTest, ProvideCurrentSessionData) {
  // Cannot fake `getPermissionSettings`. Calling histogram directly instead.
  base::UmaHistogramExactLinear(kNotifAuthorizationStatusByProviderHistogram, 2,
                                5);
  histogram_tester_->ExpectBucketCount(
      kNotifAuthorizationStatusByProviderHistogram, 2, 1);
}

TEST_F(IOSPushNotificationsMetricsProviderTest, TestContentClientPermission) {
  base::UmaHistogramBoolean(kContentNotifClientStatusByProviderHistogram, true);
  histogram_tester_->ExpectBucketCount(
      kContentNotifClientStatusByProviderHistogram, 1, 1);
}

TEST_F(IOSPushNotificationsMetricsProviderTest, TestTipsClientPermission) {
  base::UmaHistogramBoolean(kTipsNotifClientStatusByProviderHistogram, true);
  histogram_tester_->ExpectBucketCount(
      kTipsNotifClientStatusByProviderHistogram, 1, 1);
}
