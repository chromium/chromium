// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/signin/capabilities_types.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"

namespace {

void VerifySigninPromoSufficientlyVisible() {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::UpgradeSigninPromoMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Sign-in promo not visible");
}

}  // namespace

// Tests the upgrade sign-in promo restrictions.
@interface UpgradeSigninPromoTestCase : ChromeTestCase
@end

@implementation UpgradeSigninPromoTestCase

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];
  // Make sure the new tab is opened to open the upgrade sign-in promo.
  [ChromeEarlGrey openNewTab];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  // Use commandline args to insert fake policy data into NSUserDefaults. To the
  // app, this policy data will appear under the
  // "com.apple.configuration.managed" key.
  AppLaunchConfiguration config;
  config.features_enabled.push_back(switches::kForceStartupSigninPromo);
  config.additional_args.push_back(std::string("--") +
                                   switches::kEnableUpgradeSigninPromo);
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

// Tests that the sign-in promo is not visible at start-up with no identity.
// TODO(crbug.com/1442297): Need to enable this test.
- (void)DISABLED_testNoSigninPromoWithNoIdentity {
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(5));

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::UpgradeSigninPromoMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the sign-in promo is not visible at start-up once
// the user has signed in to their account previously.
// TODO(crbug.com/1442297): Need to enable this test.
- (void)DISABLED_testStartupSigninPromoUserSignedIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey setCanOfferExtendedChromeSyncPromos:YES
                                          forIdentity:fakeIdentity];

  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::UpgradeSigninPromoMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the sign-in promo is not visible at start-up for an account
// with minor mode restrictions.
- (void)testStartupSigninPromoNotShownForMinor {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGrey setCanOfferExtendedChromeSyncPromos:NO
                                          forIdentity:fakeIdentity];

  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(5));

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::UpgradeSigninPromoMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the sign-in promo is visible at start-up for regular user.
// TODO(crbug.com/1442297): Need to enable this test.
- (void)DISABLED_testStartupSigninPromoShownForNoneMinor {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGrey setCanOfferExtendedChromeSyncPromos:YES
                                          forIdentity:fakeIdentity];

  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  VerifySigninPromoSufficientlyVisible();
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSkipSigninAccessibilityIdentifier)]
      performAction:grey_tap()];
}

// Tests sign-in promo behavior in landscape. It should appears if and only if
// the device is an ipad.
// TODO(crbug.com/1442297): Need to enable this test.
- (void)DISABLED_testNoSignInPromoInLandscapeMode {
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGrey setCanOfferExtendedChromeSyncPromos:YES
                                          forIdentity:fakeIdentity];

  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  [ChromeEarlGreyUI waitForAppToIdle];

  if ([ChromeEarlGrey isIPadIdiom]) {
    VerifySigninPromoSufficientlyVisible();
  } else {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::UpgradeSigninPromoMatcher()]
        assertWithMatcher:grey_notVisible()];
  }
}

@end
