// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"

#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/signin/public/base/signin_metrics.h"
#include "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_consumer.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::SysNSStringToUTF16;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using l10n_util::GetNSString;
using l10n_util::GetNSStringF;

namespace {

class SigninPromoViewMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    user_full_name_ = @"John Doe";
    close_button_hidden_ = YES;
  }

  void TearDown() override {
    // All callbacks should be triggered to make sure tests are working
    // correctly. If this test fails,
    // |WaitUntilFakeChromeIdentityServiceCallbackCompleted| should be called
    // in the test.
    EXPECT_FALSE(ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
                     ->HasPendingCallback());
    [mediator_ signinPromoViewIsRemoved];
    EXPECT_EQ(ios::SigninPromoViewState::Invalid,
              mediator_.signinPromoViewState);
    mediator_ = nil;
    EXPECT_OCMOCK_VERIFY((id)consumer_);
    EXPECT_OCMOCK_VERIFY((id)signin_promo_view_);
    EXPECT_OCMOCK_VERIFY((id)primary_button_);
    EXPECT_OCMOCK_VERIFY((id)secondary_button_);
    EXPECT_OCMOCK_VERIFY((id)close_button_);
  }

  void CreateMediator(signin_metrics::AccessPoint accessPoint) {
    consumer_ = OCMStrictProtocolMock(@protocol(SigninPromoViewConsumer));
    mediator_ =
        [[SigninPromoViewMediator alloc] initWithBrowserState:nil
                                                  accessPoint:accessPoint
                                                    presenter:nil];
    mediator_.consumer = consumer_;

    signin_promo_view_ = OCMStrictClassMock([SigninPromoView class]);
    primary_button_ = OCMStrictClassMock([UIButton class]);
    OCMStub([signin_promo_view_ primaryButton]).andReturn(primary_button_);
    secondary_button_ = OCMStrictClassMock([UIButton class]);
    OCMStub([signin_promo_view_ secondaryButton]).andReturn(secondary_button_);
    close_button_ = OCMStrictClassMock([UIButton class]);
    OCMStub([signin_promo_view_ closeButton]).andReturn(close_button_);
  }

  // Creates the default identity and adds it into the ChromeIdentityService.
  void AddDefaultIdentity() {
    expected_default_identity_ =
        [FakeChromeIdentity identityWithEmail:@"johndoe@example.com"
                                       gaiaID:@"1"
                                         name:user_full_name_];
    ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
        ->AddIdentity(expected_default_identity_);
  }

  // Tests the mediator as a cold state with a new created configurator.
  void TestColdState() {
    EXPECT_EQ(nil, mediator_.defaultIdentity);
    CheckColdStateConfigurator([mediator_ createConfigurator]);
  }

  // Adds an identity and tests the mediator as a warm state.
  void TestWarmState() {
    // Expect to receive an update to the consumer with a configurator.
    ExpectConfiguratorNotification(YES /* identity changed */);
    AddDefaultIdentity();
    // Check the configurator received by the consumer.
    CheckWarmStateConfigurator(configurator_);
    // Check a new created configurator.
    CheckWarmStateConfigurator([mediator_ createConfigurator]);
    // The consumer should receive a notification related to the image.
    CheckForImageNotification();
  }

  // Expects a notification on the consumer for an identity update, and stores
  // the received configurator into configurator_.
  void ExpectConfiguratorNotification(BOOL identity_changed) {
    configurator_ = nil;
    SigninPromoViewConfigurator* configurator_arg =
        [OCMArg checkWithBlock:^BOOL(id value) {
          configurator_ = value;
          return YES;
        }];
    OCMExpect([consumer_
        configureSigninPromoWithConfigurator:configurator_arg
                             identityChanged:identity_changed]);
  }

  // Expects the signin promo view to be configured in a cold state.
  void ExpectColdStateConfiguration() {
    OCMExpect([signin_promo_view_ setMode:SigninPromoViewModeColdState]);
    NSString* title = GetNSString(IDS_IOS_OPTIONS_IMPORT_DATA_TITLE_SIGNIN);
    OCMExpect([signin_promo_view_ setAccessibilityLabel:title]);
    OCMExpect([primary_button_ setTitle:title forState:UIControlStateNormal]);
    image_view_profile_image_ = nil;
  }

  // Checks a cold state configurator.
  void CheckColdStateConfigurator(SigninPromoViewConfigurator* configurator) {
    EXPECT_NE(nil, configurator);
    ExpectColdStateConfiguration();
    OCMExpect([close_button_ setHidden:close_button_hidden_]);
    [configurator configureSigninPromoView:signin_promo_view_];
    EXPECT_EQ(nil, image_view_profile_image_);
  }

  // Expects the signin promo view to be configured in a warm state.
  void ExpectWarmStateConfiguration() {
    EXPECT_EQ(expected_default_identity_, mediator_.defaultIdentity);
    OCMExpect([signin_promo_view_ setMode:SigninPromoViewModeWarmState]);
    OCMExpect([signin_promo_view_
        setProfileImage:[OCMArg checkWithBlock:^BOOL(id value) {
          image_view_profile_image_ = value;
          return YES;
        }]]);
    NSString* name = expected_default_identity_.userFullName.length
                         ? expected_default_identity_.userFullName
                         : expected_default_identity_.userEmail;
    base::string16 name16 = SysNSStringToUTF16(name);
    NSString* accessibilityLabel =
        GetNSStringF(IDS_IOS_SIGNIN_PROMO_ACCESSIBILITY_LABEL, name16);
    OCMExpect([signin_promo_view_ setAccessibilityLabel:accessibilityLabel]);
    OCMExpect([primary_button_
        setTitle:GetNSStringF(IDS_IOS_SIGNIN_PROMO_CONTINUE_AS, name16)
        forState:UIControlStateNormal]);
    OCMExpect([secondary_button_
        setTitle:GetNSString(IDS_IOS_SIGNIN_PROMO_CHANGE_ACCOUNT)
        forState:UIControlStateNormal]);
  }

  // Checks a warm state configurator.
  void CheckWarmStateConfigurator(SigninPromoViewConfigurator* configurator) {
    EXPECT_NE(nil, configurator);
    ExpectWarmStateConfiguration();
    OCMExpect([close_button_ setHidden:YES]);
    [configurator configureSigninPromoView:signin_promo_view_];
    EXPECT_NE(nil, image_view_profile_image_);
  }

  // Checks to receive a notification for the image upate of the current
  // identity.
  void CheckForImageNotification() {
    configurator_ = nil;
    ExpectConfiguratorNotification(NO /* identity changed */);
    WaitUntilFakeChromeIdentityServiceCallbackCompleted();
    // Check the configurator received by the consumer.
    CheckWarmStateConfigurator(configurator_);
  }

  // Runs the runloop until all callback from FakeChromeIdentityService are
  // called.
  void WaitUntilFakeChromeIdentityServiceCallbackCompleted() {
    ConditionBlock condition = ^() {
      return !ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
                  ->HasPendingCallback();
    };
    EXPECT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition));
  }

  // Mediator used for the tests.
  SigninPromoViewMediator* mediator_;

  // User full name for the identity;
  NSString* user_full_name_;
  // Identity used for the warm state.
  FakeChromeIdentity* expected_default_identity_;

  // Configurator received from the consumer.
  SigninPromoViewConfigurator* configurator_;

  // Mocks.
  id<SigninPromoViewConsumer> consumer_;
  SigninPromoView* signin_promo_view_;
  UIButton* primary_button_;
  UIButton* secondary_button_;
  UIButton* close_button_;

  // Value set by -[SigninPromoView setProfileImage:].
  UIImage* image_view_profile_image_;
  // Value set by -[close_button_ setHidden:].
  BOOL close_button_hidden_;
};

// Tests signin promo view and its configurator in cold state.
TEST_F(SigninPromoViewMediatorTest, ColdStateConfigureSigninPromoView) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  TestColdState();
}

// Tests signin promo view and its configurator in cold state in settings view.
TEST_F(SigninPromoViewMediatorTest,
       ColdStateConfigureSigninPromoViewFromSettings) {
  close_button_hidden_ = NO;
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
  TestColdState();
}

// Tests signin promo view and its configurator in warm state.
TEST_F(SigninPromoViewMediatorTest, WarmStateConfigureSigninPromoView) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  TestWarmState();
}

// Tests signin promo view and its configurator in warm state, with an identity
// without full name.
TEST_F(SigninPromoViewMediatorTest,
       WarmStateConfigureSigninPromoViewWithoutFullName) {
  user_full_name_ = nil;
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  TestWarmState();
}

// Tests the scenario with the sign-in promo in cold state, and then adding an
// identity to update the view in warm state.
TEST_F(SigninPromoViewMediatorTest, ConfigureSigninPromoViewWithColdAndWarm) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  TestColdState();
  TestWarmState();
}

// Tests the scenario with the sign-in promo in warm state, and then removing
// the identity to update the view in cold state.
TEST_F(SigninPromoViewMediatorTest, ConfigureSigninPromoViewWithWarmAndCold) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  TestWarmState();
  // Expect to receive a new configuration from -[Consumer
  // configureSigninPromoWithConfigurator:identityChanged:].
  ExpectConfiguratorNotification(YES /* identity changed */);
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->RemoveIdentity(expected_default_identity_);
  expected_default_identity_ = nil;
  // Check the received configurator.
  CheckColdStateConfigurator(configurator_);
}

// Tests the view state before and after calling -[SigninPromoViewMediator
// signinPromoViewIsVisible].
TEST_F(SigninPromoViewMediatorTest, SigninPromoViewStateVisible) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  // Test initial state.
  EXPECT_EQ(ios::SigninPromoViewState::NeverVisible,
            mediator_.signinPromoViewState);
  [mediator_ signinPromoViewIsVisible];
  // Test state once the sign-in promo view is visible.
  EXPECT_EQ(ios::SigninPromoViewState::Unused, mediator_.signinPromoViewState);
}

// Tests the view state while signing in.
TEST_F(SigninPromoViewMediatorTest, SigninPromoViewStateSignedin) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  [mediator_ signinPromoViewIsVisible];
  __block ShowSigninCommandCompletionCallback completion;
  ShowSigninCommandCompletionCallback completion_arg =
      [OCMArg checkWithBlock:^BOOL(ShowSigninCommandCompletionCallback value) {
        completion = value;
        return YES;
      }];
  // Start sign-in.
  OCMExpect([consumer_
           signinPromoViewMediator:mediator_
      shouldOpenSigninWithIdentity:nil
                       promoAction:
                           signin_metrics::PromoAction::
                               PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT
                        completion:completion_arg]);
  [mediator_ signinPromoViewDidTapSigninWithNewAccount:signin_promo_view_];
  EXPECT_TRUE(mediator_.signinInProgress);
  EXPECT_EQ(ios::SigninPromoViewState::UsedAtLeastOnce,
            mediator_.signinPromoViewState);
  EXPECT_NE(nil, (id)completion);
  // Stop sign-in.
  OCMExpect([consumer_ signinDidFinish]);
  completion(YES);
  EXPECT_FALSE(mediator_.signinInProgress);
  EXPECT_EQ(ios::SigninPromoViewState::UsedAtLeastOnce,
            mediator_.signinPromoViewState);
}

// Tests that no update notification is sent by the mediator to its consumer,
// while the sign-in is in progress, when an identity is added.
TEST_F(SigninPromoViewMediatorTest,
       SigninPromoViewNoUpdateNotificationWhileSignin) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  [mediator_ signinPromoViewIsVisible];
  __block ShowSigninCommandCompletionCallback completion;
  ShowSigninCommandCompletionCallback completion_arg =
      [OCMArg checkWithBlock:^BOOL(ShowSigninCommandCompletionCallback value) {
        completion = value;
        return YES;
      }];
  OCMExpect([consumer_
           signinPromoViewMediator:mediator_
      shouldOpenSigninWithIdentity:nil
                       promoAction:
                           signin_metrics::PromoAction::
                               PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT
                        completion:completion_arg]);
  // Starts sign-in without identity.
  [mediator_ signinPromoViewDidTapSigninWithNewAccount:signin_promo_view_];
  // Adds an identity while doing sign-in.
  AddDefaultIdentity();
  // No consumer notification should be expected.
  WaitUntilFakeChromeIdentityServiceCallbackCompleted();
  // Finishs the sign-in.
  OCMExpect([consumer_ signinDidFinish]);
  completion(YES);
}

// Tests that no update notification is sent by the mediator to its consumer,
// while the sign-in is in progress.
TEST_F(SigninPromoViewMediatorTest,
       SigninPromoViewNoUpdateNotificationWhileSignin2) {
  AddDefaultIdentity();
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  [mediator_ signinPromoViewIsVisible];
  __block ShowSigninCommandCompletionCallback completion;
  ShowSigninCommandCompletionCallback completion_arg =
      [OCMArg checkWithBlock:^BOOL(ShowSigninCommandCompletionCallback value) {
        completion = value;
        return YES;
      }];
  OCMExpect([consumer_ signinPromoViewMediator:mediator_
                  shouldOpenSigninWithIdentity:expected_default_identity_
                                   promoAction:signin_metrics::PromoAction::
                                                   PROMO_ACTION_WITH_DEFAULT
                                    completion:completion_arg]);
  // Starts sign-in with an identity.
  [mediator_ signinPromoViewDidTapSigninWithDefaultAccount:signin_promo_view_];
  EXPECT_TRUE(
      [mediator_ conformsToProtocol:@protocol(ChromeIdentityServiceObserver)]);
  id<ChromeIdentityServiceObserver> chromeIdentityServiceObserver =
      (id<ChromeIdentityServiceObserver>)mediator_;
  // Simulates an identity update.
  [chromeIdentityServiceObserver profileUpdate:expected_default_identity_];
  // Spins the run loop to wait for the profile image update.
  WaitUntilFakeChromeIdentityServiceCallbackCompleted();
  // Finishs the sign-in.
  OCMExpect([consumer_ signinDidFinish]);
  completion(YES);
}

}  // namespace
