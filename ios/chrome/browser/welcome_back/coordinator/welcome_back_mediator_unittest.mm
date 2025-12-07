// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/welcome_back/coordinator/welcome_back_mediator.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/avatar_provider.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/browser/welcome_back/model/welcome_back_prefs.h"
#import "ios/chrome/browser/welcome_back/ui/welcome_back_screen_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

// Test class for the WelcomeBackMediator.
class WelcomeBackMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_system_identity_);

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));

    profile_ = std::move(builder).Build();
    prefs_ = profile_.get()->GetPrefs();
    AuthenticationService* auth_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_.get());

    mediator_ = [[WelcomeBackMediator alloc]
        initWithAuthenticationService:auth_service_
                accountManagerService:ChromeAccountManagerServiceFactory::
                                          GetForProfile(profile_.get())];

    consumer_ = OCMStrictProtocolMock(@protocol(WelcomeBackScreenConsumer));
  }

  void TearDown() override {
    PlatformTest::TearDown();
    mediator_.consumer = nil;
    [mediator_ disconnect];
    mediator_ = nil;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<PrefService> prefs_;
  FakeSystemIdentity* fake_system_identity_ =
      [FakeSystemIdentity fakeIdentity1];
  WelcomeBackMediator* mediator_;
  id consumer_;
};

// Tests that the preferred items are sent correctly when all the items are
// eligble.
TEST_F(WelcomeBackMediatorTest, ConfirmEligiblePreferredItemsSet) {
  // Enable Variant A: Bling’s Basics with Locked Incognito Tabs.
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kWelcomeBack, {{kWelcomeBackParam, "1"}});

  // Expect Lens, Enhanced Safe Browsing, and Locked Incognito Tabs and the
  // default title.
  OCMExpect([consumer_
      setWelcomeBackItems:[OCMArg checkWithBlock:^BOOL(
                                      NSArray<BestFeaturesItem*>* items) {
        return items[0].type == BestFeaturesItemType::kLensSearch &&
               items[1].type == BestFeaturesItemType::kEnhancedSafeBrowsing &&
               items[2].type == BestFeaturesItemType::kLockedIncognitoTabs;
      }]]);

  OCMExpect([consumer_
      setTitle:l10n_util::GetNSString(IDS_IOS_WELCOME_BACK_TITLE_DEFAULT)]);

  // Verify the correct items are set.
  mediator_.consumer = consumer_;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that an item is replaced with the highest priority item when it is
// ineligble.
TEST_F(WelcomeBackMediatorTest, ConfirmIneligibleItemReplaced) {
  // Enable Variant B: Bling’s Basics with Save & Autofill Passwords.
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kWelcomeBack, {{kWelcomeBackParam, "2"}});

  // Mark Lens as used.
  MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kLensSearch);

  // Expect Locked Incognito Tabs, Enhanced Safe Browsing, and Save & Autofill
  // Passwords and the default title.
  OCMExpect([consumer_
      setWelcomeBackItems:[OCMArg checkWithBlock:^BOOL(
                                      NSArray<BestFeaturesItem*>* items) {
        return items[0].type == BestFeaturesItemType::kEnhancedSafeBrowsing &&
               items[1].type ==
                   BestFeaturesItemType::kSaveAndAutofillPasswords &&
               items[2].type == BestFeaturesItemType::kLockedIncognitoTabs;
      }]]);
  OCMExpect([consumer_
      setTitle:l10n_util::GetNSString(IDS_IOS_WELCOME_BACK_TITLE_DEFAULT)]);

  // Verify the correct items are set.
  mediator_.consumer = consumer_;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that all preferred items are replaced if all items are ineligible.
TEST_F(WelcomeBackMediatorTest, ConfirmAllPreferredItemsReplaced) {
  // Enable Variant C: Productivity and Shopping.
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kWelcomeBack, {{kWelcomeBackParam, "3"}});

  // Mark all the preferred features as used.
  MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kTabGroups);
  MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kLockedIncognitoTabs);
  MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kPriceTrackingAndInsights);

  // Expect Lens, Enhanced Safe Browsing, and Save & Autofill Passwords and the
  // default title.
  OCMExpect([consumer_
      setWelcomeBackItems:[OCMArg checkWithBlock:^BOOL(
                                      NSArray<BestFeaturesItem*>* items) {
        return items[0].type == BestFeaturesItemType::kLensSearch &&
               items[1].type == BestFeaturesItemType::kEnhancedSafeBrowsing &&
               items[2].type == BestFeaturesItemType::kSaveAndAutofillPasswords;
      }]]);

  OCMExpect([consumer_
      setTitle:l10n_util::GetNSString(IDS_IOS_WELCOME_BACK_TITLE_DEFAULT)]);

  // Verify the correct items are set.
  mediator_.consumer = consumer_;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that an item is replaced correctly if a mix of high and low priority
// items are eligible.
TEST_F(WelcomeBackMediatorTest, ConfirmLowPriorityItemReplacement) {
  // Enable Variant A: Bling’s Basics with Locked Incognito Tabs.
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kWelcomeBack, {{kWelcomeBackParam, "1"}});

  // Mark half of the features as used.
  MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kLockedIncognitoTabs);
  MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kSaveAndAutofillPasswords);
  MarkWelcomeBackFeatureUsed(
      BestFeaturesItemType::kAutofillPasswordsInOtherApps);
  MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kSharePasswordsWithFamily);

  // Expect Lens, Enhanced Safe Browsing, and Tab Groups and the
  // default title.
  OCMExpect([consumer_
      setWelcomeBackItems:[OCMArg checkWithBlock:^BOOL(
                                      NSArray<BestFeaturesItem*>* items) {
        return items[0].type == BestFeaturesItemType::kLensSearch &&
               items[1].type == BestFeaturesItemType::kEnhancedSafeBrowsing &&
               items[2].type == BestFeaturesItemType::kTabGroups;
      }]]);

  OCMExpect([consumer_
      setTitle:l10n_util::GetNSString(IDS_IOS_WELCOME_BACK_TITLE_DEFAULT)]);

  // Verify the correct items are set.
  mediator_.consumer = consumer_;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that only two items are set to display.
TEST_F(WelcomeBackMediatorTest, ConfirmOnlyTwoItemsSet) {
  // Enable Variant A: Bling’s Basics with Locked Incognito Tabs.
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kWelcomeBack, {{kWelcomeBackParam, "1"}});

  // Mark 6 out of 8 features as used.
  MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kEnhancedSafeBrowsing);
  MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kLockedIncognitoTabs);
  MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kSaveAndAutofillPasswords);
  MarkWelcomeBackFeatureUsed(
      BestFeaturesItemType::kAutofillPasswordsInOtherApps);
  MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kSharePasswordsWithFamily);
  MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kPriceTrackingAndInsights);

  // Expect Lens and Tab Groups and the default title.
  OCMExpect([consumer_
      setWelcomeBackItems:[OCMArg checkWithBlock:^BOOL(
                                      NSArray<BestFeaturesItem*>* items) {
        return items[0].type == BestFeaturesItemType::kLensSearch &&
               items[1].type == BestFeaturesItemType::kTabGroups;
      }]]);

  OCMExpect([consumer_
      setTitle:l10n_util::GetNSString(IDS_IOS_WELCOME_BACK_TITLE_DEFAULT)]);

  // Verify the correct items are set.
  mediator_.consumer = consumer_;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that `itemsToDisplay` returns three items when only three items are
// eligible, and they are not all preferred items.
TEST_F(WelcomeBackMediatorTest, ConfirmOnlyThreeEligibleItemsSet) {
  // Enable Variant A: Bling’s Basics with Locked Incognito Tabs.
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kWelcomeBack, {{kWelcomeBackParam, "1"}});

  // Mark 5 out of 8 features as used.
  MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kLensSearch);
  MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kSaveAndAutofillPasswords);
  MarkWelcomeBackFeatureUsed(
      BestFeaturesItemType::kAutofillPasswordsInOtherApps);
  MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kSharePasswordsWithFamily);
  MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kPriceTrackingAndInsights);

  // Expect Enhanced Safe Browsing, Locked Incognito Tabs, and Tab Groups and
  // the default title.
  OCMExpect([consumer_
      setWelcomeBackItems:[OCMArg checkWithBlock:^BOOL(
                                      NSArray<BestFeaturesItem*>* items) {
        return items[0].type == BestFeaturesItemType::kEnhancedSafeBrowsing &&
               items[1].type == BestFeaturesItemType::kLockedIncognitoTabs &&
               items[2].type == BestFeaturesItemType::kTabGroups;
      }]]);

  OCMExpect([consumer_
      setTitle:l10n_util::GetNSString(IDS_IOS_WELCOME_BACK_TITLE_DEFAULT)]);

  // Verify the correct items are set.
  mediator_.consumer = consumer_;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the username and profile picture are set for signed in users.
TEST_F(WelcomeBackMediatorTest, ConfirmUserInformationRetrieved) {
  // Enable Variant D: Sign In Benefits.
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kWelcomeBack, {{kWelcomeBackParam, "4"}});

  // Sign in to a fake account.
  AuthenticationService* auth_service_ =
      AuthenticationServiceFactory::GetForProfile(profile_.get());
  auth_service_->SignIn(fake_system_identity_,
                        signin_metrics::AccessPoint::kUnknown);

  // Expect Lens, Enhanced Safe Browsing, and Autofill Passwords in Other Apps
  // and the personalized title.
  OCMExpect([consumer_
      setWelcomeBackItems:[OCMArg checkWithBlock:^BOOL(
                                      NSArray<BestFeaturesItem*>* items) {
        return items[0].type == BestFeaturesItemType::kLensSearch &&
               items[1].type == BestFeaturesItemType::kEnhancedSafeBrowsing &&
               items[2].type ==
                   BestFeaturesItemType::kAutofillPasswordsInOtherApps;
      }]]);

  // Retrieve the name and avatar.
  UIImage* testAvatar =
      GetApplicationContext()->GetIdentityAvatarProvider()->GetIdentityAvatar(
          fake_system_identity_, IdentityAvatarSize::Large);
  OCMExpect([consumer_ setAvatar:testAvatar]);
  OCMExpect([consumer_ setTitle:l10n_util::GetNSStringF(
                                    IDS_IOS_WELCOME_BACK_TITLE_SIGNED_IN,
                                    base::SysNSStringToUTF16(
                                        fake_system_identity_.userGivenName))]);

  // Verify the correct items are set.
  mediator_.consumer = consumer_;
  EXPECT_OCMOCK_VERIFY(consumer_);
}
