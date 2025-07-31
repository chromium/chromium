// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_TEST_CASE_BASE_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_TEST_CASE_BASE_H_

#import "ios/chrome/test/earl_grey/chrome_test_case.h"

@class GREYElementInteraction;
@protocol GREYMatcher;
enum class BrowserSigninMode;

// Type of FRE sign-in screen intent.
typedef NS_ENUM(NSUInteger, FRESigninIntent) {
  // FRE without enterprise policy.
  FRESigninIntentRegular,
  // FRE without forced sign-in policy.
  FRESigninIntentSigninForcedByPolicy,
  // FRE without disabled sign-in policy.
  FRESigninIntentSigninDisabledByPolicy,
  // FRE with an enterprise policy which is not explicitly handled by another
  // entry.
  FRESigninIntentSigninWithPolicy,
  // FRE with the SyncDisabled enterprise policy.
  FRESigninIntentSigninWithSyncDisabledPolicy,
  // FRE with no UMA link in the first screen.
  FRESigninIntentSigninWithUMAReportingDisabledPolicy,
};

// Type of FRE default browser screen intent.
enum class FREDefaultBrowserIntent {
  // FRE without enterprise policy.
  kRegular,
  // FRE with enterprise policy and UMA disabled.
  kEnterpriseWithoutUMADisclaimer,
};

// Base test class for egtests for first run.
@interface FirstRunTestCaseBase : ChromeTestCase

// Dismisses the default browser screen.
+ (void)dismissDefaultBrowser;

// Dismisses the remaining screens in FRE after the default browser screen.
+ (void)dismissDefaultBrowserAndRemainingScreens;

// Returns GREYElementInteraction for `matcher`, using `scrollViewMatcher` to
// scroll.
- (GREYElementInteraction*)
    elementInteractionWithGreyMatcher:(id<GREYMatcher>)matcher
                 scrollViewIdentifier:(NSString*)scrollViewIdentifier;

// Checks that the sign-in screen for enterprise is displayed.
- (void)verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
    (FRESigninIntent)FRESigninIntent;

// Checks that the default browser screen is displayed with the correct
// disclaimer.
- (void)verifyDefaultBrowserIsDisplayedWithScreenIntent:
    (FREDefaultBrowserIntent)screenIntent;

// Accept the history opt-in screen.
- (void)acceptSyncOrHistory;

- (void)verifySyncOrHistoryEnabled:(BOOL)enabled;

// Checks the disclaimer footer with the list of strings. `strings` can contain
// "BEGIN_LINK" and "END_LINK" for URL tags.
- (void)verifyDisclaimerFooterWithStrings:(NSArray*)strings;

- (void)relaunchAppWithBrowserSigninMode:(BrowserSigninMode)mode;

// Sets policy value and relaunches the app.
- (void)relaunchAppWithPolicyKey:(std::string)policyKey
                  xmlPolicyValue:(std::string)xmlPolicyValue;

// Checks that the default browser screen is displayed.
- (void)verifyDefaultBrowserIsDisplayed;

// Checks that the default browser screen is not displayed.
- (void)verifyDefaultBrowserNotDisplayed;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_TEST_CASE_BASE_H_
