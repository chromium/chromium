// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_account_context_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_consumer.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_item_identifier.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using set_up_list_prefs::SetUpListItemState;

// Tests the PushNotificationsOptInMediator functionality.
class NotificationsOptInMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    ProfileIOS* profile =
        profile_manager_.AddProfileWithBuilder(CreateProfileBuilder());

    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile, std::make_unique<FakeAuthenticationServiceDelegate>());
    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile);
    prefs_ = profile->GetPrefs();
    scoped_feature_list_.InitWithFeatures(
        {kIOSTipsNotifications, kContentPushNotifications}, {});
    consumer_ = OCMStrictProtocolMock(@protocol(NotificationsOptInConsumer));
  }

  void TearDown() override {
    prefs_->ClearPref(prefs::kFeaturePushNotificationPermissions);
    local_state()->ClearPref(prefs::kAppLevelPushNotificationPermissions);
  }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

 protected:
  // Enables/disables notifications with `key`.
  void TurnNotificationForKey(BOOL on, const std::string key) {
    ScopedDictPrefUpdate update(prefs_.get(),
                                prefs::kFeaturePushNotificationPermissions);
    update->Set(key, on);
  }

  // Enables/disables app level notifications with `key`.
  void TurnAppLevelNotificationForKey(BOOL on, const std::string key) {
    ScopedDictPrefUpdate update(local_state(),
                                prefs::kAppLevelPushNotificationPermissions);
    update->Set(key, on);
  }

  // Builds a profile.
  TestProfileIOS::Builder CreateProfileBuilder() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    return builder;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<AuthenticationService> auth_service_ = nullptr;
  NotificationsOptInMediator* mediator_;
  id consumer_;
};

// Tests that the mediator makes the proper consumer calls when all the
// notifications are initially disabled.
TEST_F(NotificationsOptInMediatorTest,
       TestConsumer_NotificationsInitiallyDisabled) {
  mediator_ = [[NotificationsOptInMediator alloc]
      initWithAuthenticationService:auth_service_];
  OCMExpect([consumer_ setOptInItem:kTips enabled:NO]);
  OCMExpect([consumer_ setOptInItem:kContent enabled:NO]);
  OCMExpect([consumer_ setOptInItem:kPriceTracking enabled:NO]);

  mediator_.consumer = consumer_;
  [mediator_ configureConsumer];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the mediator makes the proper consumer calls when all the
// notifications are intially enabled and then one switch is disabled.
TEST_F(NotificationsOptInMediatorTest,
       TestConsumer_NotificationsInitiallyEnabled) {
  TurnNotificationForKey(YES, kCommerceNotificationKey);
  TurnNotificationForKey(YES, kContentNotificationKey);
  TurnNotificationForKey(YES, kSportsNotificationKey);
  TurnAppLevelNotificationForKey(YES, kTipsNotificationKey);

  mediator_ = [[NotificationsOptInMediator alloc]
      initWithAuthenticationService:auth_service_];
  OCMExpect([consumer_ setOptInItem:kTips enabled:YES]);
  OCMExpect([consumer_ setOptInItem:kContent enabled:YES]);
  OCMExpect([consumer_ setOptInItem:kPriceTracking enabled:YES]);

  mediator_.consumer = consumer_;
  [mediator_ configureConsumer];
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_ setOptInItem:kTips enabled:NO]);
  [mediator_ disableUserSelectionForItem:kTips];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that tapping "No Thanks" marks the Set Up List item as complete.
TEST_F(NotificationsOptInMediatorTest, TestNoThanksTapped) {
  mediator_ = [[NotificationsOptInMediator alloc]
      initWithAuthenticationService:auth_service_];
  [mediator_ didTapSecondaryActionButton];
  SetUpListItemState item_state = set_up_list_prefs::GetItemState(
      local_state(), SetUpListItemType::kNotifications);
  EXPECT_TRUE(item_state == SetUpListItemState::kCompleteInList ||
              item_state == SetUpListItemState::kCompleteNotInList);
}
