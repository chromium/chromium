// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/policy/policy_constants.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/first_run/first_run_app_interface.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::AdvancedSyncSettingsDoneButtonMatcher;
using chrome_test_util::SettingsLink;
using chrome_test_util::MatchInWindowWithNumber;
using chrome_test_util::MatchInBlockerWindowWithNumber;
using chrome_test_util::FakeOmnibox;

namespace {

// Returns matcher for the opt in accept button.
id<GREYMatcher> FirstRunOptInAcceptButton() {
  return ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_FIRSTRUN_OPT_IN_ACCEPT_BUTTON));
}

// Returns matcher for the skip sign in button.
id<GREYMatcher> SkipSigninButton() {
  return grey_accessibilityID(kSkipSigninAccessibilityIdentifier);
}
}

// Tests legacy first run settings and navigation.
@interface LegacyFirstRunTestCase : ChromeTestCase
@end

@implementation LegacyFirstRunTestCase

- (void)setUp {
  [[self class] testForStartup];

  [super setUp];
  [FirstRunAppInterface setUMACollectionEnabled:NO];
  [FirstRunAppInterface resetUMACollectionEnabledByDefault];
}

- (void)tearDown {
  [PolicyAppInterface clearPolicies];
  [FirstRunAppInterface setUMACollectionEnabled:NO];
  [FirstRunAppInterface resetUMACollectionEnabledByDefault];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_disabled.push_back(kEnableFREUIModuleIOS);

  // Show the First Run UI at startup.
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");

  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByKilling;

  return config;
}

// Navigates to the terms of service and back.
- (void)testTermsAndConditions {
  id<GREYMatcher> termsOfServiceLink =
      grey_accessibilityLabel(@"Terms of Service");
  [[EarlGrey selectElementWithMatcher:termsOfServiceLink]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_FIRSTRUN_TERMS_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"ic_arrow_back"),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];

  // Ensure we went back to the First Run screen.
  [[EarlGrey selectElementWithMatcher:termsOfServiceLink]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Ensure that we have completed First Runs.
  // Accept the FRE.
  [[EarlGrey selectElementWithMatcher:FirstRunOptInAcceptButton()]
      performAction:grey_tap()];
  // Dismiss sign-in.
  [[EarlGrey selectElementWithMatcher:SkipSigninButton()]
      performAction:grey_tap()];
}

// Toggle the UMA checkbox.
- (void)testToggleMetricsOn {
  id<GREYMatcher> metrics =
      grey_accessibilityID(first_run::kUMAMetricsButtonAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:metrics] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:FirstRunOptInAcceptButton()]
      performAction:grey_tap()];

  GREYAssertNotEqual([FirstRunAppInterface isUMACollectionEnabled],
                     [FirstRunAppInterface isUMACollectionEnabledByDefault],
                     @"Metrics reporting pref is incorrect.");

  // Ensure that we have completed First Run, otherwise Earl Grey test crashes
  // on check that the sign-in coordinator is no longer running.
  [[EarlGrey selectElementWithMatcher:SkipSigninButton()]
      performAction:grey_tap()];
}

// Dismisses the first run screens.
- (void)testDismissFirstRun {
  [[EarlGrey selectElementWithMatcher:FirstRunOptInAcceptButton()]
      performAction:grey_tap()];

  GREYAssertEqual([FirstRunAppInterface isUMACollectionEnabled],
                  [FirstRunAppInterface isUMACollectionEnabledByDefault],
                  @"Metrics reporting does not match.");

  [[EarlGrey selectElementWithMatcher:SkipSigninButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Signs in to an account and then taps the Advanced link to go to settings.
- (void)testSignInAndTapSettingsLink {
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Launch First Run and accept tems of services.
  [[EarlGrey selectElementWithMatcher:FirstRunOptInAcceptButton()]
      performAction:grey_tap()];

  // Tap Settings link.
  [[EarlGrey selectElementWithMatcher:SettingsLink()] performAction:grey_tap()];

  // Check Sync hasn't started yet, allowing the user to change some
  //  settings.
  GREYAssertFalse([FirstRunAppInterface isSyncFirstSetupComplete],
                  @"Sync shouldn't have finished its original setup yet");

  // Close Settings, user is still signed in and sync shouldn't start.
  [[EarlGrey selectElementWithMatcher:AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];

  // Check sync did not start.
  GREYAssertFalse([FirstRunAppInterface isSyncFirstSetupComplete],
                  @"Sync shouldn't start when discarding advanced settings.");

  // Tap "Yes, I'm in." button
  [SigninEarlGreyUI tapSigninConfirmationDialog];

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Check sync started.
  GREYAssertTrue([FirstRunAppInterface isSyncFirstSetupComplete],
                 @"Sync should have finished its original setup");
}

// Checks that the sync screen doesn't appear when the SyncDisabled policy is
// enabled.
- (void)testSyncDisabled {
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);

  // Launch First Run and accept tems of services.
  [[EarlGrey selectElementWithMatcher:FirstRunOptInAcceptButton()]
      performAction:grey_tap()];

  // The SignIn screen should not be displayed, so the NTP should be visible.
  [[EarlGrey selectElementWithMatcher:FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks FRE shows in only one window.
- (void)testFirstRunInMultiWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  [[EarlGrey selectElementWithMatcher:MatchInWindowWithNumber(
                                          0, grey_accessibilityLabel(
                                                 @"Terms of Service"))]
      assertWithMatcher:grey_notNil()];

  // Check UI Blocked in second window and that message is a button.
  [[EarlGrey
      selectElementWithMatcher:
          MatchInBlockerWindowWithNumber(
              1,
              grey_text(l10n_util::GetNSString(
                  IDS_IOS_UI_BLOCKED_USE_OTHER_WINDOW_SWITCH_WINDOW_ACTION)))]
      assertWithMatcher:grey_allOf(
                            grey_ancestor(grey_kindOfClassName(@"UIButton")),
                            grey_sufficientlyVisible(), nil)];

  // Finish FRE.
  [[EarlGrey selectElementWithMatcher:MatchInWindowWithNumber(
                                          0, FirstRunOptInAcceptButton())]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:MatchInWindowWithNumber(0, SkipSigninButton())]
      performAction:grey_tap()];

  // Check for both fake omniboxes visibility.
  [[EarlGrey selectElementWithMatcher:MatchInWindowWithNumber(0, FakeOmnibox())]
      assertWithMatcher:grey_sufficientlyVisible()];

  // TODO(crbug.com/1169687) enable following test once EG2 bug for multiwindow
  //    grey_sufficientlyVisible is fixed.
  // [[EarlGrey selectElementWithMatcher:MatchInWindowWithNumber(1,
  // FakeOmnibox())]
  //  assertWithMatcher:grey_sufficientlyVisible()];
}

@end
