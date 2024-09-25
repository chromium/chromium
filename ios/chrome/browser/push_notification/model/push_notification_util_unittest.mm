// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_util.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util+testing.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/platform_test.h"

namespace {

const char kAuthorizationStatusHistogramName[] =
    "IOS.PushNotification.NotificationSettingsAuthorizationStatus";

const char kNotificationAutorizationStatusChangedToAuthorized[] =
    "IOS.PushNotification.NotificationAutorizationStatusChangedToAuthorized";

const char kNotificationAutorizationStatusChangedToDenied[] =
    "IOS.PushNotification.NotificationAutorizationStatusChangedToDenied";

NSDictionary<NSString*, id>* testPayload =
    @{@"$" : @{@"n" : @"a:content_push_notify:RANDOM_ID"}};
}  // namespace

class PushNotificationUtilTest : public PlatformTest {
 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::HistogramTester histogram_tester_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

// Ensures that the proper UMA histogram and bucket are logged when
// `[PushNotificationUtil logPermissionSettingsMetrics:]` is invoked.
TEST_F(PushNotificationUtilTest, loggingAuthorizationStatus) {
  [PushNotificationUtil
      logPermissionSettingsMetrics:UNAuthorizationStatusNotDetermined];
  histogram_tester_.ExpectBucketCount(
      kAuthorizationStatusHistogramName,
      push_notification::SettingsAuthorizationStatus::NOTDETERMINED, 1);
  histogram_tester_.ExpectTotalCount(kAuthorizationStatusHistogramName, 1);

  [PushNotificationUtil
      logPermissionSettingsMetrics:UNAuthorizationStatusDenied];
  histogram_tester_.ExpectBucketCount(
      kAuthorizationStatusHistogramName,
      push_notification::SettingsAuthorizationStatus::DENIED, 1);
  histogram_tester_.ExpectTotalCount(kAuthorizationStatusHistogramName, 2);

  [PushNotificationUtil
      logPermissionSettingsMetrics:UNAuthorizationStatusAuthorized];
  histogram_tester_.ExpectBucketCount(
      kAuthorizationStatusHistogramName,
      push_notification::SettingsAuthorizationStatus::AUTHORIZED, 1);
  histogram_tester_.ExpectTotalCount(kAuthorizationStatusHistogramName, 3);

  [PushNotificationUtil
      logPermissionSettingsMetrics:UNAuthorizationStatusProvisional];
  histogram_tester_.ExpectBucketCount(
      kAuthorizationStatusHistogramName,
      push_notification::SettingsAuthorizationStatus::PROVISIONAL, 1);
  histogram_tester_.ExpectTotalCount(kAuthorizationStatusHistogramName, 4);

  [PushNotificationUtil
      logPermissionSettingsMetrics:UNAuthorizationStatusEphemeral];
  histogram_tester_.ExpectBucketCount(
      kAuthorizationStatusHistogramName,
      push_notification::SettingsAuthorizationStatus::EPHEMERAL, 1);
  histogram_tester_.ExpectTotalCount(kAuthorizationStatusHistogramName, 5);
}

// Ensures that the proper UMA histograms and buckets are logged when
// changing authorization status' to authorized or denied.
TEST_F(PushNotificationUtilTest, loggingAuthorizationStatusChange) {
  [PushNotificationUtil
      updateAuthorizationStatusPref:UNAuthorizationStatusDenied];
  histogram_tester_.ExpectBucketCount(
      kNotificationAutorizationStatusChangedToDenied,
      push_notification::SettingsAuthorizationStatus::NOTDETERMINED, 1);
  histogram_tester_.ExpectTotalCount(
      kNotificationAutorizationStatusChangedToDenied, 1);
  // A repeat request should not be logged to UMA.
  [PushNotificationUtil
      updateAuthorizationStatusPref:UNAuthorizationStatusDenied];
  histogram_tester_.ExpectTotalCount(
      kNotificationAutorizationStatusChangedToDenied, 1);

  [PushNotificationUtil
      updateAuthorizationStatusPref:UNAuthorizationStatusAuthorized];
  histogram_tester_.ExpectBucketCount(
      kNotificationAutorizationStatusChangedToAuthorized,
      push_notification::SettingsAuthorizationStatus::DENIED, 1);
  histogram_tester_.ExpectTotalCount(
      kNotificationAutorizationStatusChangedToAuthorized, 1);
  // A repeat request should not be logged to UMA.
  [PushNotificationUtil
      updateAuthorizationStatusPref:UNAuthorizationStatusAuthorized];
  histogram_tester_.ExpectTotalCount(
      kNotificationAutorizationStatusChangedToAuthorized, 1);
}

// Test the client id Mapping for stable chime client.
TEST_F(PushNotificationUtilTest, mappingClientIds) {
  ASSERT_EQ([PushNotificationUtil
                mapToPushNotificationClientIdFromUserInfo:testPayload],
            PushNotificationClientId::kContent);
}

// Test the client id mapping for unstable chime client.
TEST_F(PushNotificationUtilTest, mappingUnsableClientIds) {
  ASSERT_EQ([PushNotificationUtil
                mapToPushNotificationClientIdFromUserInfo:testPayload],
            PushNotificationClientId::kContent);
}
