// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/chrome_switches.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_constants.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Matcher for the sign-in recall promo.
id<GREYMatcher> SigninRecallPromo() {
  return grey_accessibilityID(kUnifiedConsentScrollViewIdentifier);
}

void VerifySigninPromoSufficientlyVisible() {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:SigninRecallPromo()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Sign-in promo not visible");
}

AppLaunchConfiguration AppConfigurationForRelaunch() {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(switches::kForceStartupSigninPromo);
  config.features_disabled.push_back(switches::kMinorModeSupport);
  config.additional_args.push_back(std::string("--") +
                                   switches::kEnableSigninRecallPromo);

  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByKilling;
  return config;
}

}  // namespace

// Tests the upgrade sign-in promo restrictions.
@interface UpgradeSigninPromoTestCase : ChromeTestCase
@end

@implementation UpgradeSigninPromoTestCase

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];
}

// Tests that the sign-in promo is visible at start-up.
- (void)testStartupSigninPromoNoRestrictions {
  // Create the config to relaunch Chrome.
  AppLaunchConfiguration config = AppConfigurationForRelaunch();

  // Relaunch the app to take the configuration into account.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  VerifySigninPromoSufficientlyVisible();
}

// Tests that the sign-in promo is not visible at start-up once
// the user has signed in to their account previously.
- (void)testStartupSigninPromoUserSignedIn {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Create the config to relaunch Chrome.
  AppLaunchConfiguration config = AppConfigurationForRelaunch();
  // Add the switch to make sure that fakeIdentity1 is known at startup to avoid
  // automatic sign out.
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kSignInAtStartup);

  // Relaunch the app to take the configuration into account.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [[EarlGrey selectElementWithMatcher:SigninRecallPromo()]
      assertWithMatcher:grey_notVisible()];
}

@end
