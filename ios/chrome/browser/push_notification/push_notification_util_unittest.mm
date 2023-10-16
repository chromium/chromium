// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/push_notification_util.h"

#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/push_notification/push_notification_util+testing.h"
#import "testing/platform_test.h"

namespace {
enum class PushNotificationSettingsAuthorizationStatus {
  NOTDETERMINED,
  DENIED,
  AUTHORIZED,
  PROVISIONAL,
  EPHEMERAL,
  kMaxValue = EPHEMERAL
};

const char kAuthorizationStatusHistogramName[] =
    "IOS.PushNotification.NotificationSettingsAuthorizationStatus";
}  // namespace

class PushNotificationUtilTest : public PlatformTest {
 protected:
  base::HistogramTester histogram_tester_;
};

// Ensures that the proper UMA histogram and bucket are logged when
// `[PushNotificationUtil logPermissionSettingsMetrics:]` is invoked.
TEST_F(PushNotificationUtilTest, loggingAuthorizationStatus) {
  [PushNotificationUtil
      logPermissionSettingsMetrics:UNAuthorizationStatusNotDetermined];
  histogram_tester_.ExpectBucketCount(
      kAuthorizationStatusHistogramName,
      PushNotificationSettingsAuthorizationStatus::NOTDETERMINED, 1);
  histogram_tester_.ExpectTotalCount(kAuthorizationStatusHistogramName, 1);

  [PushNotificationUtil
      logPermissionSettingsMetrics:UNAuthorizationStatusDenied];
  histogram_tester_.ExpectBucketCount(
      kAuthorizationStatusHistogramName,
      PushNotificationSettingsAuthorizationStatus::DENIED, 1);
  histogram_tester_.ExpectTotalCount(kAuthorizationStatusHistogramName, 2);

  [PushNotificationUtil
      logPermissionSettingsMetrics:UNAuthorizationStatusAuthorized];
  histogram_tester_.ExpectBucketCount(
      kAuthorizationStatusHistogramName,
      PushNotificationSettingsAuthorizationStatus::AUTHORIZED, 1);
  histogram_tester_.ExpectTotalCount(kAuthorizationStatusHistogramName, 3);

  [PushNotificationUtil
      logPermissionSettingsMetrics:UNAuthorizationStatusProvisional];
  histogram_tester_.ExpectBucketCount(
      kAuthorizationStatusHistogramName,
      PushNotificationSettingsAuthorizationStatus::PROVISIONAL, 1);
  histogram_tester_.ExpectTotalCount(kAuthorizationStatusHistogramName, 4);

  [PushNotificationUtil
      logPermissionSettingsMetrics:UNAuthorizationStatusEphemeral];
  histogram_tester_.ExpectBucketCount(
      kAuthorizationStatusHistogramName,
      PushNotificationSettingsAuthorizationStatus::EPHEMERAL, 1);
  histogram_tester_.ExpectTotalCount(kAuthorizationStatusHistogramName, 5);
}
