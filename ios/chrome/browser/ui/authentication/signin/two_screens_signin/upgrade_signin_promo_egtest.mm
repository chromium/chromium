// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
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
        selectElementWithMatcher:chrome_test_util::SigninScreenPromoMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Sign-in promo not visible");
}

void VerifyHystoryOptInPromoSufficientlyVisible() {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"History opt-in promo not visible");
}

// Opens an NTP, sends Chrome to background and brings it back to foreground.
// The upgrade promo can be triggered when Chrome moves to foreground and
// nothing is displayed on the tab (so the tab grid must not be opened).
void OpenNTPAndBackgroundAndForegroundApp() {
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI waitForAppToIdle];
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  [ChromeEarlGreyUI waitForAppToIdle];
}

}  // namespace

// Tests the upgrade sign-in promo restrictions.
@interface UpgradeSigninPromoTestCase : ChromeTestCase
@end

@implementation UpgradeSigninPromoTestCase

- (void)setUp {
  [super setUp];
  [[self class] testForStartup];
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  [ChromeEarlGrey
      removeUserDefaultsObjectForKey:kDisplayedSSORecallPromoCountKey];
  [ChromeEarlGrey
      removeUserDefaultsObjectForKey:kDisplayedSSORecallForMajorVersionKey];
  [ChromeEarlGrey
      removeUserDefaultsObjectForKey:kLastShownAccountGaiaIdVersionKey];
  [ChromeEarlGrey
      removeUserDefaultsObjectForKey:kSigninPromoViewDisplayCountKey];
}

- (void)tearDown {
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  // Use commandline args to insert fake policy data into NSUserDefaults. To the
  // app, this policy data will appear under the
  // "com.apple.configuration.managed" key.
  AppLaunchConfiguration config;
  config.features_enabled.push_back(switches::kForceStartupSigninPromo);
  config.additional_args.push_back(std::string("--") +
                                   switches::kEnableUpgradeSigninPromo);
  // Without relaunch upgrade signin promo will not be shown again.
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

// Tests that the sign-in promo is not visible at start-up with no identity.
- (void)testNoSigninPromoWithNoIdentity {
  OpenNTPAndBackgroundAndForegroundApp();

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SigninScreenPromoMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the history opt-in promo is shown if the user is signed in to
// an account without history sync.
// TODO(crbug.com/346537324): Test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testHistoryOptInPromoUserSignedIn \
  testHistoryOptInPromoUserSignedIn
#else
#define MAYBE_testHistoryOptInPromoUserSignedIn \
  DISABLED_testHistoryOptInPromoUserSignedIn
#endif
- (void)MAYBE_testHistoryOptInPromoUserSignedIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableHistorySync:NO];

  OpenNTPAndBackgroundAndForegroundApp();

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SigninScreenPromoMatcher()]
      assertWithMatcher:grey_notVisible()];
  VerifyHystoryOptInPromoSufficientlyVisible();
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  [self expectUpgradePromoMetricsAndPreferences];
}

- (void)testHistoryOptInPromoNotShownWhenAlreadyGranted {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableHistorySync:YES];

  OpenNTPAndBackgroundAndForegroundApp();

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SigninScreenPromoMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the sign-in promo is visible at start-up for regular user, and
// followed by the history sync opt-in.
// TODO(crbug.com/346537324): Test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testStartupSigninPromoShown testStartupSigninPromoShown
#else
#define MAYBE_testStartupSigninPromoShown DISABLED_testStartupSigninPromoShown
#endif
- (void)MAYBE_testStartupSigninPromoShown {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  OpenNTPAndBackgroundAndForegroundApp();

  VerifySigninPromoSufficientlyVisible();
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  [self expectUpgradePromoMetricsAndPreferences];
}

// Tests sign-in promo behavior in landscape. It should appears if and only if
// the device is an ipad.
// TODO(crbug.com/40266894): Need to enable this test.
- (void)DISABLED_testNoSignInPromoInLandscapeMode {
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  OpenNTPAndBackgroundAndForegroundApp();

  if ([ChromeEarlGrey isIPadIdiom]) {
    VerifySigninPromoSufficientlyVisible();
  } else {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::SigninScreenPromoMatcher()]
        assertWithMatcher:grey_notVisible()];
  }
}

#pragma mark - Helpers

- (void)expectUpgradePromoMetricsAndPreferences {
  NSError* error = [MetricsAppInterface
      expectUniqueSampleWithCount:1
                        forBucket:1
                     forHistogram:base::SysUTF8ToNSString(
                                      kUMASSORecallAccountsAvailable)];
  GREYAssertNil(error, @"Failed to record show count histogram %s %@",
                kUMASSORecallAccountsAvailable, error);
  error = [MetricsAppInterface
      expectUniqueSampleWithCount:1
                        forBucket:1
                     forHistogram:base::SysUTF8ToNSString(
                                      kUMASSORecallPromoSeenCount)];
  GREYAssertNil(error, @"Failed to record show count histogram %s %@",
                kUMASSORecallPromoSeenCount, error);
  error = [MetricsAppInterface
      expectUniqueSampleWithCount:1
                        forBucket:PromoActionEnabledSSOAccount
                     forHistogram:base::SysUTF8ToNSString(
                                      kUMASSORecallPromoAction)];
  GREYAssertNil(error, @"Failed to record show count histogram %s %@",
                kUMASSORecallPromoAction, error);
  NSNumber* value =
      [ChromeEarlGrey userDefaultsObjectForKey:kSigninPromoViewDisplayCountKey];
  GREYAssertEqual(1, value.integerValue, @"Failed to increase %@ pref",
                  kSigninPromoViewDisplayCountKey);
  NSArray* gaiaIds = [ChromeEarlGrey
      userDefaultsObjectForKey:kLastShownAccountGaiaIdVersionKey];
  // It is not possible to do `GREYAssertEqualObjects(expectedGaiaIds, gaiaIds),
  // since gaiaIds is EDOObject type (the object is in Chrome app).
  GREYAssertEqual(1, gaiaIds.count, @"Expect to have only one gaia id %@",
                  gaiaIds);
  GREYAssertEqualObjects([FakeSystemIdentity fakeIdentity1].gaiaID, gaiaIds[0],
                         @"Wrong gaia id in %@",
                         kLastShownAccountGaiaIdVersionKey);
}

@end
