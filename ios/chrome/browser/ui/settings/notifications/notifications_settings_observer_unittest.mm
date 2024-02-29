// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/notifications/notifications_settings_observer.h"

#import "base/test/task_environment.h"
#import "components/commerce/core/pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_settings_observer.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Tests NotificationsSettingsObserver functionality.
class NotificationsSettingsObserverTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kFeaturePushNotificationPermissions);
    pref_service_->registry()->RegisterBooleanPref(
        commerce::kPriceEmailNotificationsEnabled, false);

    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    local_state_->registry()->RegisterDictionaryPref(
        prefs::kAppLevelPushNotificationPermissions);

    observer_ = [[NotificationsSettingsObserver alloc]
        initWithPrefService:pref_service_.get()
                 localState:local_state_.get()];
    observer_.delegate = mock_delegate_;
  }

  void TearDown() override {
    [observer_ disconnect];
    PlatformTest::TearDown();
  }

  void TurnOnNotificationForKey(const std::string key) {
    ScopedDictPrefUpdate update(pref_service_.get(),
                                prefs::kFeaturePushNotificationPermissions);
    update->Set(key, true);
  }

  void TurnOnAppLevelNotificationForKey(const std::string key) {
    ScopedDictPrefUpdate update(local_state_.get(),
                                prefs::kAppLevelPushNotificationPermissions);
    update->Set(key, true);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  NotificationsSettingsObserver* observer_;
  id<NotificationsSettingsObserverDelegate> mock_delegate_ =
      OCMProtocolMock(@protocol(NotificationsSettingsObserverDelegate));
};

// Tests the observer delegate is notified with correct client id when pref
// kPriceEmailNotificationsEnabled changed.
TEST_F(NotificationsSettingsObserverTest,
       PrefkPriceEmailNotificationsEnabledChanged) {
  OCMExpect([mock_delegate_ notificationsSettingsDidChangeForClient:
                                PushNotificationClientId::kCommerce]);
  pref_service_->SetBoolean(commerce::kPriceEmailNotificationsEnabled, true);
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests the observer delegate is notified with correct client id when
// kFeaturePushNotificationPermissions pref changed.
TEST_F(NotificationsSettingsObserverTest,
       PrefkFeaturePushNotificationPermissionsChanged) {
  OCMExpect([mock_delegate_ notificationsSettingsDidChangeForClient:
                                PushNotificationClientId::kCommerce]);
  TurnOnNotificationForKey(kCommerceNotificationKey);
  EXPECT_OCMOCK_VERIFY(mock_delegate_);

  OCMExpect([mock_delegate_ notificationsSettingsDidChangeForClient:
                                PushNotificationClientId::kContent]);
  TurnOnNotificationForKey(kContentNotificationKey);
  EXPECT_OCMOCK_VERIFY(mock_delegate_);

  OCMExpect([mock_delegate_ notificationsSettingsDidChangeForClient:
                                PushNotificationClientId::kSports]);
  TurnOnNotificationForKey(kSportsNotificationKey);
  EXPECT_OCMOCK_VERIFY(mock_delegate_);

  OCMExpect([mock_delegate_
      notificationsSettingsDidChangeForClient:PushNotificationClientId::kTips]);
  TurnOnAppLevelNotificationForKey(kTipsNotificationKey);
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}
