// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

// EarlGrey tests for the Level Up feature.
@interface LevelUpTestCase : ChromeTestCase
@end

@implementation LevelUpTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled.push_back(kIOSLevelUp);
  return config;
}

- (void)setUp {
  [super setUp];
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kLevelUpOptIn];
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kLevelUpUIEnabled];
}

- (void)tearDownHelper {
  [ChromeEarlGrey closeAllExtraWindows];
  [ChromeEarlGrey clearUserPrefWithName:prefs::kLevelUpOptIn];
  [ChromeEarlGrey clearUserPrefWithName:prefs::kLevelUpUIEnabled];
  [ChromeEarlGrey clearUserPrefWithName:prefs::kLevelUpCompletedTasks];
  [SigninEarlGrey signOut];
  [super tearDownHelper];
}

// Test that completing the Incognito task in one window does not show the
// completion snackbar in another non-Incognito window on iPad multiwindow.
- (void)testIncognitoTaskSnackbarNotShownInMultiwindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  // Open a second window (window 1) with a regular tab.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [ChromeEarlGrey openNewTabInWindowWithNumber:1];

  // Open an Incognito tab in the first window (window 0).
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(0)];
  [ChromeEarlGrey openNewIncognitoTab];

  // Check across all windows that the Incognito task completion snackbar is
  // not visible.
  [EarlGrey setRootMatcherForSubsequentInteractions:nil];
  NSString* snackbarText =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_TASK_COMPLETED_INCOGNITO);
  [[EarlGrey selectElementWithMatcher:grey_text(snackbarText)]
      assertWithMatcher:grey_nil()];
}

@end
