// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/public/features.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/public/quick_delete_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "ui/base/l10n/l10n_util_mac.h"

using chrome_test_util::BrowsingDataButtonMatcher;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ClearBrowsingDataView;
using testing::NavigationBarBackButton;

namespace {

// Returns a matcher for the title of the Quick Delete Browsing Data page.
id<GREYMatcher> QuickDeleteBrowsingDataPageTitleMatcher() {
  return chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
      IDS_IOS_DELETE_BROWSING_DATA_TITLE);
}

// Returns a matcher for the title of the Quick Delete Other Data page.
id<GREYMatcher> QuickDeleteOtherDataPageTitleMatcher() {
  return chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
      IDS_SETTINGS_OTHER_GOOGLE_DATA_TITLE);
}

// Returns a matcher for the "Manage other data" cell.
id<GREYMatcher> ManageOtherDataCellMatcher() {
  return grey_accessibilityID(kQuickDeleteManageOtherDataCellIdentifier);
}

// Returns a matcher for the "Passwords and passkeys" cell.
id<GREYMatcher> PasswordsAndPasskeysCellMatcher() {
  return grey_accessibilityID(
      kQuickDeleteOtherDataPasswordsAndPasskeysIdentifier);
}

// Returns a matcher for the "Search history" cell.
id<GREYMatcher> SearchHistoryCellMatcher() {
  return grey_accessibilityID(kQuickDeleteOtherDataSearchHistoryIdentifier);
}

// Returns a matcher for the "My Activity" cell.
id<GREYMatcher> MyActivityCellMatcher() {
  return grey_accessibilityID(kQuickDeleteOtherDataMyActivityIdentifier);
}

// Returns a matcher for the footer.
id<GREYMatcher> FooterMatcher() {
  return grey_accessibilityID(kQuickDeleteOtherDataFooterIdentifier);
}

// Opens the Quick Delete Other Data page.
void OpenQuickDeleteOtherDataPage() {
  [ChromeEarlGreyUI openToolsMenu];

  [ChromeEarlGreyUI
      tapToolsMenuAction:ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                             IDS_IOS_TOOLS_MENU_CLEAR_BROWSING_DATA))];

  [[EarlGrey selectElementWithMatcher:BrowsingDataButtonMatcher()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      QuickDeleteBrowsingDataPageTitleMatcher()];

  [[EarlGrey selectElementWithMatcher:ManageOtherDataCellMatcher()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      QuickDeleteOtherDataPageTitleMatcher()];
}

}  // namespace

// Tests the Quick Delete Other Data page.
@interface QuickDeleteOtherDataTestCase : ChromeTestCase
@end

@implementation QuickDeleteOtherDataTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kPasswordRemovalFromDeleteBrowsingData);

  return config;
}

// Tests that the back button dismisses the Quick Delete Other Data Page.
- (void)testPageNavigationBackButton {
  // Open the Quick Delete Other Data page.
  OpenQuickDeleteOtherDataPage();

  // Tap the back button.
  [[EarlGrey selectElementWithMatcher:NavigationBarBackButton()]
      performAction:grey_tap()];

  // Ensure the Quick Delete Other Data page is closed while the Quick Delete
  // Browsing Data page is still open.
  [[EarlGrey selectElementWithMatcher:QuickDeleteOtherDataPageTitleMatcher()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:QuickDeleteBrowsingDataPageTitleMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the dismissal of the Quick Delete Other Data page when swiped down.
- (void)testPageDismissalViaDownSwipe {
  // Open the Quick Delete Other Data page.
  OpenQuickDeleteOtherDataPage();

  // Swipe the Quick Delete Other Data page down.
  [[EarlGrey selectElementWithMatcher:QuickDeleteOtherDataPageTitleMatcher()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Ensure the Quick Delete Other Data page is closed while the quick delete
  // bottom sheet is still open.
  [[EarlGrey selectElementWithMatcher:QuickDeleteOtherDataPageTitleMatcher()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the "Passwords and passkeys", "Search history" and the "My
// Activity" cells are visible. It also verifies that the footer is visible.
- (void)testTableViewVisibility {
  // Open the Quick Delete Other Data page.
  OpenQuickDeleteOtherDataPage();

  [[EarlGrey selectElementWithMatcher:PasswordsAndPasskeysCellMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SearchHistoryCellMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:MyActivityCellMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:FooterMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
