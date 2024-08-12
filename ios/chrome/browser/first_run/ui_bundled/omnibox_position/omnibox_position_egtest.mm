// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "components/search_engines/search_engines_switches.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/omnibox_position_choice_app_interface.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

/// Label of the top address bar option.
NSString* TopAddressBarLabelText() {
  return l10n_util::GetNSString(IDS_IOS_TOP_ADDRESS_BAR_OPTION);
}

/// Label of the bottom address bar option.
NSString* BottomAddressBarLabelText() {
  return l10n_util::GetNSString(IDS_IOS_BOTTOM_ADDRESS_BAR_OPTION);
}

/// Matcher for the top address bar option.
id<GREYMatcher> TopAddressBarOption() {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      TopAddressBarLabelText());
}

/// Matcher for the bottom address bar option.
id<GREYMatcher> BottomAddressBarOption() {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      BottomAddressBarLabelText());
}

/// Matcher for the top address bar option when selected.
id<GREYMatcher> TopAddressBarOptionSelected() {
  return grey_allOf(grey_selected(), TopAddressBarOption(), nil);
}

/// Matcher for the bottom address bar option when selected.
id<GREYMatcher> BottomAddressBarOptionSelected() {
  return grey_allOf(grey_selected(), BottomAddressBarOption(), nil);
}

/// Returns GREYElementInteraction for `matcher`, using `scrollViewMatcher` to
/// scroll.
void TapPromoStyleButton(NSString* buttonIdentifier) {
  id<GREYMatcher> buttonMatcher = grey_accessibilityID(buttonIdentifier);
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kPromoStyleScrollViewAccessibilityIdentifier);
  // Needs to scroll slowly to make sure to not miss a cell if it is not
  // currently on the screen. It should not be bigger than the visible part
  // of the collection view.
  id<GREYAction> searchAction = grey_scrollInDirection(kGREYDirectionDown, 200);
  GREYElementInteraction* element =
      [[EarlGrey selectElementWithMatcher:buttonMatcher]
             usingSearchAction:searchAction
          onElementWithMatcher:scrollViewMatcher];
  [element performAction:grey_tap()];
}

}  // namespace

/// Tests the omnibox position choice screen.
@interface OmniboxPositionTestCase : ChromeTestCase
@end

@implementation OmniboxPositionTestCase

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey resetDataForLocalStatePref:prefs::kBottomOmnibox];
}

#pragma mark Tests

// Tests selecting top omnibox when top is selected by default.
- (void)testSelectTopWithTopDefault {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPad (no choice for omnibox position on tablet)");
  }
  [OmniboxPositionChoiceAppInterface showOmniboxPositionChoiceScreen];

  // Verify that the Omnibox Position choice screen is showing.
  id<GREYMatcher> omniboxPositionView = grey_accessibilityID(
      first_run::kFirstRunOmniboxPositionChoiceScreenAccessibilityIdentifier);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:omniboxPositionView];

  // The top address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:TopAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  // Confirm selection.
  TapPromoStyleButton(kPromoStylePrimaryActionAccessibilityIdentifier);

  // Verify that the preferred omnibox position is top.
  GREYAssertFalse([ChromeEarlGrey localStateBooleanPref:prefs::kBottomOmnibox],
                  @"Failed to set preferred omnibox position to top");
}

// Tests selecting bottom omnibox when top is selected by default.
- (void)testSelectBottomWithTopDefault {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPad (no choice for omnibox position on tablet)");
  }
  [OmniboxPositionChoiceAppInterface showOmniboxPositionChoiceScreen];

  // Verify that the Omnibox Position choice screen is showing.
  id<GREYMatcher> omniboxPositionView = grey_accessibilityID(
      first_run::kFirstRunOmniboxPositionChoiceScreenAccessibilityIdentifier);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:omniboxPositionView];

  // The top address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:TopAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  // Tap on the bottom address bar option.
  [[EarlGrey selectElementWithMatcher:BottomAddressBarOption()]
      performAction:grey_tap()];

  // The bottom address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:BottomAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  // Confirm selection.
  TapPromoStyleButton(kPromoStylePrimaryActionAccessibilityIdentifier);

  // Verify that the preferred omnibox position is bottom.
  GREYAssertTrue([ChromeEarlGrey localStateBooleanPref:prefs::kBottomOmnibox],
                 @"Failed to set preferred omnibox position to bottom");
}

/// Tests confirming the default omnibox.
- (void)testConfirmDefaultOption {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPad (no choice for omnibox position on tablet)");
  }

  [OmniboxPositionChoiceAppInterface showOmniboxPositionChoiceScreen];

  // Verify that the Omnibox Position choice screen is showing.
  id<GREYMatcher> omniboxPositionView = grey_accessibilityID(
      first_run::kFirstRunOmniboxPositionChoiceScreenAccessibilityIdentifier);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:omniboxPositionView];

  // The top address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:TopAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  // Confirm selection.
  TapPromoStyleButton(kPromoStylePrimaryActionAccessibilityIdentifier);

  // Verify that the preferred omnibox position is top.
  GREYAssertFalse([ChromeEarlGrey localStateBooleanPref:prefs::kBottomOmnibox],
                  @"Failed to set preferred omnibox position to top");
}

/// Tests discarding the omnibox position choice promo.
- (void)testNoThanks {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPad (no choice for omnibox position on tablet)");
  }
  [OmniboxPositionChoiceAppInterface showOmniboxPositionChoiceScreen];

  // Verify that the Omnibox Position choice screen is showing.
  id<GREYMatcher> omniboxPositionView = grey_accessibilityID(
      first_run::kFirstRunOmniboxPositionChoiceScreenAccessibilityIdentifier);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:omniboxPositionView];

  // The top address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:TopAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  // Discard selection.
  TapPromoStyleButton(kPromoStyleSecondaryActionAccessibilityIdentifier);

  // Verify that there is no user preferred omnibox position.
  GREYAssertTrue(
      [ChromeEarlGrey prefWithNameIsDefaultValue:prefs::kBottomOmnibox],
      @"Failed to discard the selected position");
}

@end
