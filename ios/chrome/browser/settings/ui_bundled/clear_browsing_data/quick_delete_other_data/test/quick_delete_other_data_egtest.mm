// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/public/features.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/public/quick_delete_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
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
using chrome_test_util::NavigationBarTitleWithAccessibilityLabelId;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SettingsSearchEngineButton;
using testing::NavigationBarBackButton;

namespace {

// Returns a matcher for the title of the Quick Delete Browsing Data page.
id<GREYMatcher> QuickDeleteBrowsingDataPageTitleMatcher() {
  return NavigationBarTitleWithAccessibilityLabelId(
      IDS_IOS_DELETE_BROWSING_DATA_TITLE);
}

// Returns a matcher for the title of the Quick Delete Other Data page.
id<GREYMatcher> QuickDeleteOtherDataPageTitleMatcher(bool is_dse_google) {
  return NavigationBarTitleWithAccessibilityLabelId(
      is_dse_google ? IDS_SETTINGS_OTHER_GOOGLE_DATA_TITLE
                    : IDS_SETTINGS_OTHER_DATA_TITLE);
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
void OpenQuickDeleteOtherDataPage(bool is_dse_google) {
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

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:QuickDeleteOtherDataPageTitleMatcher(
                                              is_dse_google)];
}

// Sets which cells should be expected to be visible.
void ExpectCellVisibilities(bool passwords_and_passkeys_cell,
                            bool search_history_cell,
                            bool my_activity_cell) {
  [[EarlGrey selectElementWithMatcher:PasswordsAndPasskeysCellMatcher()]
      assertWithMatcher:passwords_and_passkeys_cell ? grey_sufficientlyVisible()
                                                    : grey_nil()];

  [[EarlGrey selectElementWithMatcher:SearchHistoryCellMatcher()]
      assertWithMatcher:search_history_cell ? grey_sufficientlyVisible()
                                            : grey_nil()];

  [[EarlGrey selectElementWithMatcher:MyActivityCellMatcher()]
      assertWithMatcher:my_activity_cell ? grey_sufficientlyVisible()
                                         : grey_nil()];
}

}  // namespace

// Tests the Quick Delete Other Data page.
@interface QuickDeleteOtherDataTestCase : ChromeTestCase
@end

@implementation QuickDeleteOtherDataTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kPasswordRemovalFromDeleteBrowsingData);

  // Set the regulatory country to Canada.
  config.additional_args.push_back("--search-engine-choice-country=CA");

  return config;
}

// Triggers sign-in of the user.
- (void)signIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
}

// Sets the default search engine to DuckDuckGo.
- (void)setDefaultSearchEngineToDuckDuckGo {
  // Set the default search engine to the second one in the list (non-Google).
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsSearchEngineButton()];

  // Match the cell containing the text "DuckDuckGo".
  id<GREYMatcher> duckDuckGoMatcher = grey_accessibilityLabel(@"DuckDuckGo");

  // Ensure the cell is visible, scrolling if necessary.
  // We assume the cells are within a UITableView.
  [[[EarlGrey selectElementWithMatcher:duckDuckGoMatcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_kindOfClass([UITableView class])]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the back button dismisses the Quick Delete Other Data Page.
- (void)testPageNavigationBackButton {
  // Open the Quick Delete Other Data page.
  OpenQuickDeleteOtherDataPage(/*is_dse_google=*/YES);

  // Tap the back button.
  [[EarlGrey selectElementWithMatcher:NavigationBarBackButton()]
      performAction:grey_tap()];

  // Ensure the Quick Delete Other Data page is closed while the Quick Delete
  // Browsing Data page is still open.
  [[EarlGrey selectElementWithMatcher:QuickDeleteOtherDataPageTitleMatcher(YES)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:QuickDeleteBrowsingDataPageTitleMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the dismissal of the Quick Delete Other Data page when swiped down.
- (void)testPageDismissalViaDownSwipe {
  // Open the Quick Delete Other Data page.
  OpenQuickDeleteOtherDataPage(/*is_dse_google=*/YES);

  // Swipe the Quick Delete Other Data page down.
  [[EarlGrey selectElementWithMatcher:QuickDeleteOtherDataPageTitleMatcher(
                                          /*is_dse_google=*/YES)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Ensure the Quick Delete Other Data page is closed while the quick delete
  // bottom sheet is still open.
  [[EarlGrey selectElementWithMatcher:QuickDeleteOtherDataPageTitleMatcher(
                                          /*is_dse_google=*/YES)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the "Passwords and passkeys", "Search history" and the "My
// Activity" cells are visible. It also verifies that the footer is visible.
- (void)testTableViewVisibility {
  [self signIn];
  // Open the Quick Delete Other Data page.
  OpenQuickDeleteOtherDataPage(/*is_dse_google=*/YES);

  ExpectCellVisibilities(/*passwords_and_passkeys_cell=*/YES,
                         /*search_history_cell=*/YES, /*my_activity_cell=*/YES);

  [[EarlGrey selectElementWithMatcher:FooterMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the "My Activity" and "Search history" cells are hidden when the
// user is signed out and Google is the default search engine.
- (void)testPageWithSignedOutUser {
  // Open the Quick Delete Other Data page.
  OpenQuickDeleteOtherDataPage(/*is_dse_google=*/YES);

  ExpectCellVisibilities(/*passwords_and_passkeys_cell=*/YES,
                         /*search_history_cell=*/NO, /*my_activity_cell=*/NO);
}

// Tests that the "My Activity" cell is hidden when the user is signed out and
// Google is not the default search engine.
- (void)testPageWithSignedOutUserAndNonGoogleSearchEngine {
  [self setDefaultSearchEngineToDuckDuckGo];
  // Open the Quick Delete Other Data page.
  OpenQuickDeleteOtherDataPage(/*is_dse_google=*/NO);

  ExpectCellVisibilities(/*passwords_and_passkeys_cell=*/YES,
                         /*search_history_cell=*/YES, /*my_activity_cell=*/NO);
}

// Tests that all three cells are visible when the user is signed in and Google
// is not the default search engine.
- (void)testPageWithSignedInUserAndNonGoogleSearchEngine {
  [self setDefaultSearchEngineToDuckDuckGo];
  [self signIn];
  // Open the Quick Delete Other Data page.
  OpenQuickDeleteOtherDataPage(/*is_dse_google=*/NO);

  ExpectCellVisibilities(/*passwords_and_passkeys_cell=*/YES,
                         /*search_history_cell=*/YES, /*my_activity_cell=*/YES);
}

// Tests that the table view updates dynamically if the user sign-in status
// changes.
- (void)testTableViewVisibilityWithSignInChanges {
  // Open the Quick Delete Other Data page.
  OpenQuickDeleteOtherDataPage(/*is_dse_google=*/YES);

  ExpectCellVisibilities(/*passwords_and_passkeys_cell=*/YES,
                         /*search_history_cell=*/NO, /*my_activity_cell=*/NO);

  [self signIn];

  ExpectCellVisibilities(/*passwords_and_passkeys_cell=*/YES,
                         /*search_history_cell=*/YES, /*my_activity_cell=*/YES);
}

@end
