// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "base/threading/platform_thread.h"
#import "base/time/time.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
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

// Enable the feature and background/foreground the app to lock incognito tabs.
- (void)displayBlockingUI {
  [ChromeEarlGrey setBoolValue:YES
             forLocalStatePref:prefs::kIncognitoAuthenticationSetting];
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
}

// Tests that the TabGrid is correctly updated when the incognito reauth screen
// is presented.
- (void)testTabGridButton {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey openNewTab];

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

// Tests that long pressing on a grid item behind the scrim doesn't trigger a
// context menu.
- (void)testContextMenuInGrid {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey openNewTab];

  [self displayBlockingUI];

  [ChromeEarlGreyUI openTabGrid];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];

  NSString* cellID =
      [NSString stringWithFormat:@"%@%u", kGridCellIdentifierPrefix, 0];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(cellID),
                                          grey_ancestor(grey_accessibilityID(
                                              kIncognitoTabGridIdentifier)),
                                          nil)]
      performAction:chrome_test_util::LongPressOnHiddenElement()];

  base::PlatformThread::Sleep(base::Seconds(1));
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::CloseTabMenuButton(), nil)]
      assertWithMatcher:grey_nil()];
}

@end
