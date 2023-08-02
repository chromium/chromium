// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/metrics_util.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#import "ios/chrome/common/app_group/app_group_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace credential_provider_extension {

NSString* kMetric1 = @"CpeMetricTest1";
NSString* kMetric2 = @"CpeMetricTest2";

void RemoveMetricForKey(NSString* key) {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  [sharedDefaults removeObjectForKey:key];
}

void VerifyMetricForKey(NSString* key, int valueExpected) {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSInteger value = [sharedDefaults integerForKey:key];
  ASSERT_EQ(value, valueExpected);
}

class MetricsUtilTest : public PlatformTest {
 public:
  void SetUp() override;
  void TearDown() override;
};

void MetricsUtilTest::SetUp() {
  RemoveMetricForKey(kMetric1);
  RemoveMetricForKey(kMetric2);
}

void MetricsUtilTest::TearDown() {
  RemoveMetricForKey(kMetric1);
  RemoveMetricForKey(kMetric2);
}

// Tests increase and reset of metrics.
TEST_F(MetricsUtilTest, CheckUMACountUpdatesAsExpected) {
  VerifyMetricForKey(kMetric1, 0);
  VerifyMetricForKey(kMetric2, 0);
  UpdateUMACountForKey(kMetric1);
  VerifyMetricForKey(kMetric1, 1);
  VerifyMetricForKey(kMetric2, 0);
  UpdateUMACountForKey(kMetric2);
  UpdateUMACountForKey(kMetric2);
  VerifyMetricForKey(kMetric1, 1);
  VerifyMetricForKey(kMetric2, 2);
  RemoveMetricForKey(kMetric2);
  VerifyMetricForKey(kMetric1, 1);
  VerifyMetricForKey(kMetric2, 0);
}

}  // credential_provider_extension
