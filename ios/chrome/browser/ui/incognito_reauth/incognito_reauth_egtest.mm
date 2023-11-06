// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "ui/base/l10n/l10n_util.h"

@interface IncognitoReauthTestCase : ChromeTestCase
@end

@implementation IncognitoReauthTestCase

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kIncognitoAuthenticationSetting];
}

- (void)tearDown {
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kIncognitoAuthenticationSetting];
  [super tearDown];
}

// Adds an incognito tabs, go back to non-incognito, enable the feature and
// background/foreground the app to lock incognito tabs.
- (void)displayBlockingUI {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey setBoolValue:YES
             forLocalStatePref:prefs::kIncognitoAuthenticationSetting];
  XCUIApplication* currentApplication = [[XCUIApplication alloc] init];
  // Tell the system to background the app.
  [[XCUIDevice sharedDevice] pressButton:XCUIDeviceButtonHome];
  BOOL (^conditionBlock)(void) = ^BOOL {
    return currentApplication.state == XCUIApplicationStateRunningBackground ||
           currentApplication.state ==
               XCUIApplicationStateRunningBackgroundSuspended;
  };
  GREYCondition* condition =
      [GREYCondition conditionWithName:@"check if backgrounded"
                                 block:conditionBlock];
  GREYAssertTrue([condition waitWithTimeout:10.0 pollInterval:0.5],
                 @"Failed to background application.");
  [currentApplication activate];
}

// Tests that the TabGrid is correctly updated when the incognito reauth screen
// is presented.
- (void)testTabGridButton {
  [self displayBlockingUI];
  [ChromeEarlGreyUI openTabGrid];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridNewIncognitoTabButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridEditButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_notVisible()];

  // Label of the button used to reauth.
  NSString* buttonLabel = l10n_util::GetNSStringF(
      IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON_VOICEOVER_LABEL,
      base::SysNSStringToUTF16(BiometricAuthenticationTypeString()));
  [[EarlGrey selectElementWithMatcher:testing::ButtonWithAccessibilityLabel(
                                          buttonLabel)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
