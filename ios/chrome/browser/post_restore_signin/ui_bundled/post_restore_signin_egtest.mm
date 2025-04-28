// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/authentication/ui_bundled/expected_signin_histograms.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_matchers.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The primary identity.
FakeSystemIdentity* const kPrimaryIdentity = [FakeSystemIdentity fakeIdentity1];

}  // namespace

@interface PostRestoreSigninTestCase : ChromeTestCase
@end

@implementation PostRestoreSigninTestCase

- (void)setUp {
  [super setUp];

  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];

  // Create the config to relaunch Chrome without `kPrimaryIdentity` to simulate
  // the restore.
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  config.additional_args.push_back(std::string("-") +
                                   test_switches::kSimulatePostDeviceRestore);
  // The post-restore signin alert is a promo, and promo are implemented as IPH.
  // All IPH are disabled by default in EGtests. This flag enable this
  // particular IPH.
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kEnableIPH +
                                   "=IPH_iOSPromoPostRestore");

  // Relaunch the app to take the configuration into account.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);

  [SigninEarlGrey verifySignedOut];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_POST_RESTORE_SIGN_IN_ALERT_PROMO_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Test that continue opens the add account view.
- (void)testContinue {
  // Tap on 'Continue' to present the add account view.
  [[EarlGrey
      selectElementWithMatcher:
          grey_text(l10n_util::GetNSString(
              IDS_IOS_POST_RESTORE_SIGN_IN_FULLSCREEN_PRIMARY_ACTION_SHORT))]
      performAction:grey_tap()];

  // Add back `kPrimaryIdentity`.
  [SigninEarlGreyUI addFakeAccountInFakeAddAccountMenu:kPrimaryIdentity];

  // Decline History Sync.
  [[[EarlGrey selectElementWithMatcher:chrome_test_util::
                                           PromoScreenSecondaryButtonMatcher()]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      performAction:grey_tap()];

  ExpectedSigninHistograms* expecteds = [[ExpectedSigninHistograms alloc]
      initWithAccessPoint:signin_metrics::AccessPoint::
                              kPostDeviceRestoreSigninPromo];
  expecteds.signinSigninStartedAccessPoint = 1;
  expecteds.signinSignInStarted = 1;
  expecteds.signinSignInCompleted = 1;
  // TODO(crbug.com/41493423): We should log that the signin was offered.
  [SigninEarlGrey assertExpectedSigninHistograms:expecteds];
}

// Test that cancel does not opens the add account view.
- (void)testCancel {
  // Tap on 'Cancel' to present the add account view.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_POST_RESTORE_SIGN_IN_ALERT_PROMO_CANCEL_ACTION))]
      performAction:grey_tap()];

  // Ensure the fake add-account menu is
  // not displayed. The absence of the "add
  // account" accessibility button on screen verifies that the screen
  // was not shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthCancelButtonIdentifier)]
      assertWithMatcher:grey_nil()];

  ExpectedSigninHistograms* expecteds = [[ExpectedSigninHistograms alloc]
      initWithAccessPoint:signin_metrics::AccessPoint::
                              kPostDeviceRestoreSigninPromo];
  // TODO(crbug.com/41493423): We should log that the signin was offered.
  [SigninEarlGrey assertExpectedSigninHistograms:expecteds];
}

@end
