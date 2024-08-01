// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check_notifications/model/safety_check_notification_client.h"

#import <UserNotifications/UserNotifications.h>

#import <memory>

#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/platform_test.h"

class SafetyCheckNotificationClientTest : public PlatformTest {
 public:
  void SetUp() override {
    notification_client_ = std::make_unique<SafetyCheckNotificationClient>();

    ScopedDictPrefUpdate update(GetApplicationContext()->GetLocalState(),
                                prefs::kAppLevelPushNotificationPermissions);

    update->Set(kSafetyCheckNotificationKey, true);
  }

 protected:
  std::unique_ptr<SafetyCheckNotificationClient> notification_client_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

#pragma mark - Test cases

// Tests that HandleNotificationReception does nothing and returns "NoData".
TEST_F(SafetyCheckNotificationClientTest,
       HandleNotificationReceptionReturnsNoData) {
  EXPECT_EQ(notification_client_->HandleNotificationReception(nil),
            UIBackgroundFetchResultNoData);
}

// Tests that RegisterActionalableNotifications returns an empty array.
TEST_F(SafetyCheckNotificationClientTest,
       RegisterActionableNotificationsReturnsEmptyArray) {
  EXPECT_EQ(notification_client_->RegisterActionableNotifications().count, 0u);
}
