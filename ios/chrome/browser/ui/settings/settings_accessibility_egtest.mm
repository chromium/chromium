// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ContentSettingsButton;
using chrome_test_util::PaymentMethodsButton;
using chrome_test_util::SettingsCollectionView;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SettingsMenuPrivacyButton;
using chrome_test_util::VoiceSearchButton;

namespace {

// Matcher for the Clear Browsing Data cell on the Privacy screen.
id<GREYMatcher> ClearBrowsingDataCell() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
}
// Matcher for the Search Engine cell on the main Settings screen.
id<GREYMatcher> SearchEngineButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_SEARCH_ENGINE_SETTING_TITLE);
}
// Matcher for the addresses cell on the main Settings screen.
id<GREYMatcher> AddressesButton() {
  return ButtonWithAccessibilityLabelId(IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE);
}
// Matcher for the Google Chrome cell on the main Settings screen.
id<GREYMatcher> GoogleChromeButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_PRODUCT_NAME);
}
// Matcher for the Preload Webpages button on the bandwidth UI.
id<GREYMatcher> BandwidthPreloadWebpagesButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_OPTIONS_PRELOAD_WEBPAGES);
}
// Matcher for the Privacy Handoff button on the privacy UI.
id<GREYMatcher> PrivacyHandoffButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES);
}
// Matcher for the Privacy Block Popups button on the privacy UI.
id<GREYMatcher> BlockPopupsButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_BLOCK_POPUPS);
}
// Matcher for the Bandwidth Settings button on the main Settings screen.
id<GREYMatcher> BandwidthSettingsButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_BANDWIDTH_MANAGEMENT_SETTINGS);
}

}  // namespace

// Settings accessibility tests for Chrome.
@interface SettingsAccessibilityTestCase : ChromeTestCase
@end

@implementation SettingsAccessibilityTestCase

- (void)tearDown {
  // It is possible for a test to fail with a menu visible, which can cause
  // future tests to fail.

  // Check if a sub-menu is still displayed. If so, close it.
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      assertWithMatcher:grey_notNil()
                  error:&error];
  if (!error) {
    [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
        performAction:grey_tap()];
  }

  // Check if the Settings menu is displayed. If so, close it.
  error = nil;
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      assertWithMatcher:grey_notNil()
                  error:&error];
  if (!error) {
    [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
        performAction:grey_tap()];
  }

  [super tearDown];
}

// Closes a sub-settings menu, and then the general Settings menu.
- (void)closeSubSettingsMenu {
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Verifies that Settings opens when signed-out and in Incognito mode.
// This tests that crbug.com/607335 has not regressed.
- (void)testSettingsSignedOutIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Verifies that the Settings screen can be swiped down to dismiss, and clean up
// is performed allowing a new presentation.
- (void)testSettingsSwipeDownDismiss {
  [ChromeEarlGreyUI openSettingsMenu];

  // Check that Settings is presented.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that Settings has been dismissed.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_nil()];

  // Re-Open Settings to confirm SwipeDown cleaned up properly and Settings can
  // be shown again.
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_notNil()];
}

// Verifies the UI elements are accessible on the Settings page.
- (void)testAccessibilityOnSettingsPage {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Verifies the UI elements are accessible on the Content Settings page.
- (void)testAccessibilityOnContentSettingsPage {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:ContentSettingsButton()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Content Settings
// Block Popups page.
- (void)testAccessibilityOnContentSettingsBlockPopupsPage {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:ContentSettingsButton()];
  [[EarlGrey selectElementWithMatcher:BlockPopupsButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Privacy Settings page.
- (void)testAccessibilityOnPrivacySettingsPage {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Privacy Handoff Settings
// page.
- (void)testAccessibilityOnPrivacyHandoffSettingsPage {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [[EarlGrey selectElementWithMatcher:PrivacyHandoffButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Privacy Clear Browsing Data
// Settings page.
- (void)testAccessibilityOnPrivacyClearBrowsingHistoryPage {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI tapPrivacyMenuButton:ClearBrowsingDataCell()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Bandwidth Management Settings
// page.
- (void)testAccessibilityOnBandwidthManagementSettingsPage {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:BandwidthSettingsButton()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Bandwidth Preload Webpages
// Settings page.
- (void)testAccessibilityOnBandwidthPreloadWebpagesSettingsPage {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:BandwidthSettingsButton()];
  [[EarlGrey selectElementWithMatcher:BandwidthPreloadWebpagesButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Search engine page.
- (void)testAccessibilityOnSearchEngine {
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:SearchEngineButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the payment methods page.
- (void)testAccessibilityOnPaymentMethods {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PaymentMethodsButton()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the addresses page.
- (void)testAccessibilityOnAddresses {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:AddressesButton()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the About Chrome page.
- (void)testAccessibilityOnGoogleChrome {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleChromeButton()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Voice Search page.
- (void)testAccessibilityOnVoiceSearch {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:VoiceSearchButton()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

@end
