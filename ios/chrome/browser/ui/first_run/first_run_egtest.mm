// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/first_run/first_run_app_interface.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test first run stages
@interface FirstRunTestCase : ChromeTestCase
@end

@implementation FirstRunTestCase

- (void)setUp {
  [[self class] testForStartup];

  [super setUp];
  [FirstRunAppInterface setUMACollectionEnabled:NO];
  [FirstRunAppInterface resetUMACollectionEnabledByDefault];
}

- (void)tearDown {
  [super tearDown];
  [FirstRunAppInterface setUMACollectionEnabled:NO];
  [FirstRunAppInterface resetUMACollectionEnabledByDefault];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_disabled.push_back(kLocationPermissionsPrompt);

  config.features_enabled.push_back(kEnableFREUIModuleIOS);

  // Show the First Run UI at startup.
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");

  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByKilling;

  return config;
}

#pragma mark - Helpers

// Checks that the welcome screen is displayed.
- (void)verifyWelcomeScreenIsDisplayed {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunWelcomeScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Checks that the sign in screen is displayed.
- (void)verifySignInScreenIsDisplayed {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Checks that the sync screen is displayed.
- (void)verifySyncScreenIsDisplayed {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSyncScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

#pragma mark - Tests

// Checks that the Welcome screen is displayed correctly.
- (void)testWelcomeScreenUI {
  [self verifyWelcomeScreenIsDisplayed];

  NSString* expectedTitle =
      [ChromeEarlGrey isIPadIdiom]
          ? l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPAD)
          : l10n_util::GetNSString(
                IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPHONE);
  [[EarlGrey selectElementWithMatcher:grey_text(expectedTitle)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_WELCOME_SCREEN_SUBTITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRICS_CONSENT))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_FIRST_RUN_WELCOME_SCREEN_ACCEPT_BUTTON))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that the Sign In screen is displayed correctly.
- (void)testSignInScreenUI {
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_FIRST_RUN_WELCOME_SCREEN_ACCEPT_BUTTON))]
      performAction:grey_tap()];

  [self verifySignInScreenIsDisplayed];

  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_FIRST_RUN_SIGNIN_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Navigates to the Terms of Service and back.
- (void)testTermsAndConditions {
  // Tap on “Terms of Service” on the first screen
  [self verifyWelcomeScreenIsDisplayed];
  id<GREYMatcher> termsOfServiceLink =
      grey_accessibilityLabel(@"Terms of Service");
  [[EarlGrey selectElementWithMatcher:termsOfServiceLink]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_FIRSTRUN_TERMS_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on “Done” on the ToS screen
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Ensure we went back to the First Run screen.
  [self verifyWelcomeScreenIsDisplayed];

  // And that the button is working.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_FIRST_RUN_WELCOME_SCREEN_ACCEPT_BUTTON))]
      performAction:grey_tap()];

  [self verifySignInScreenIsDisplayed];
}

@end
