// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

NSString* TopAddressBarLabelText() {
  return l10n_util::GetNSString(IDS_IOS_TOP_ADDRESS_BAR_OPTION);
}

NSString* BottomAddressBarLabelText() {
  return l10n_util::GetNSString(IDS_IOS_BOTTOM_ADDRESS_BAR_OPTION);
}

// Matcher for Address bar setting button.
id<GREYMatcher> AddressBarSettingButton(NSString* expectedDetailText) {
  return grey_allOf(grey_text(expectedDetailText),
                    grey_ancestor(chrome_test_util::SettingsAddressBarButton()),
                    nil);
}

// Matcher for the top address bar option.
id<GREYMatcher> TopAddressBarOption() {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      TopAddressBarLabelText());
}
// Matcher for the bottom address bar option.
id<GREYMatcher> BottomAddressBarOption() {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      BottomAddressBarLabelText());
}
// Matcher for the top address bar option when selected.
id<GREYMatcher> TopAddressBarOptionSelected() {
  return grey_allOf(grey_selected(), TopAddressBarOption(), nil);
}
// Matcher for the bottom address bar option when selected.
id<GREYMatcher> BottomAddressBarOptionSelected() {
  return grey_allOf(grey_selected(), BottomAddressBarOption(), nil);
}

}  // namespace

@interface AddressBarPreferenceTestCase : ChromeTestCase
@end

@implementation AddressBarPreferenceTestCase

- (void)setUp {
  [super setUp];
  // Resets the address bar position preference to be on top.
  [ChromeEarlGrey setBoolValue:NO forLocalStatePref:prefs::kBottomOmnibox];
}

- (void)tearDown {
  // Resets the address bar position preference to be on top.
  [ChromeEarlGrey setBoolValue:NO forLocalStatePref:prefs::kBottomOmnibox];
  [super tearDown];
}

// Tests that when we select the bottom address bar view. It becomes selected
// and it actually changes the location of the address bar to the bottom.
- (void)testSelectBottomAddressBar {
  // The test is skipped on ipads because the setting only exist on iphones.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad.");
  }

  [ChromeEarlGrey loadURL:GURL("about:blank")];
  // The address bar should be on top.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxOnTop()]
      assertWithMatcher:grey_notNil()];

  [self openAddressBarPreferenceSettingPage];

  // The top address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:TopAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  // Tap on the bottom address bar option.
  [[EarlGrey selectElementWithMatcher:BottomAddressBarOption()]
      performAction:grey_tap()];

  // The bottom address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:BottomAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  // The address bar should be now on bottom.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxAtBottom()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxOnTop()]
      assertWithMatcher:grey_nil()];
}

// Tests that when we select the bottom addres bar view and go back to the
// setting page. We expect the detail text on the address bar item to change to
// 'Bottom'.
- (void)testSelectBottomAddressBarAndGoBack {
  // The test is skipped on ipads because the setting only exist on iphones.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad.");
  }

  [self openAddressBarPreferenceSettingPage];

  // The top address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:TopAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  // Tap on the bottom address bar option.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                   BottomAddressBarLabelText())]
      performAction:grey_tap()];

  // The bottom address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:BottomAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  // Go back to settings menu.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];

  // The address bar setting button should be displayed with the 'Bottom' label
  // on its detail text
  [[EarlGrey selectElementWithMatcher:AddressBarSettingButton(
                                          BottomAddressBarLabelText())]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

#pragma mark - helpers

// Opens the address bar preference setting page.
- (void)openAddressBarPreferenceSettingPage {
  [ChromeEarlGreyUI openSettingsMenu];

  // The address bar setting button should be displayed with the 'Top' label
  // on its detail text.
  [[EarlGrey selectElementWithMatcher:AddressBarSettingButton(
                                          TopAddressBarLabelText())]
      assertWithMatcher:grey_notNil()];

  // Open the address bar setting page.
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsAddressBarButton()];
}

@end
