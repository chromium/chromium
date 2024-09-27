// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/base/command_line_switches.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using browsing_data::DeleteBrowsingDataDialogAction;
using chrome_test_util::BrowsingDataButtonMatcher;
using chrome_test_util::BrowsingDataConfirmButtonMatcher;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ClearAutofillButton;
using chrome_test_util::ClearBrowsingDataView;
using chrome_test_util::ClearBrowsingHistoryButton;
using chrome_test_util::ClearCacheButton;
using chrome_test_util::ClearCookiesButton;
using chrome_test_util::ClearSavedPasswordsButton;

// Returns a matcher for the title of the Quick Delete Browsing Data page.
id<GREYMatcher> quickDeleteBrowsingDataPageTitleMatcher() {
  return chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
      IDS_IOS_DELETE_BROWSING_DATA_TITLE);
}

// Returns matcher for an element with or without the
// UIAccessibilityTraitSelected accessibility trait depending on `selected`.
id<GREYMatcher> elementIsSelectedMatcher(bool selected) {
  return selected
             ? grey_accessibilityTrait(UIAccessibilityTraitSelected)
             : grey_not(grey_accessibilityTrait(UIAccessibilityTraitSelected));
}

// Returns a matcher for the tabs cell.
id<GREYMatcher> tabsCellMatcher() {
  return grey_allOf(
      grey_accessibilityID(kQuickDeleteBrowsingDataTabsIdentifier),
      grey_sufficientlyVisible(), nil);
}

// Matcher for sign out link in the footer.
id<GREYMatcher> SignOutLinkMatcher() {
  return grey_allOf(
      // The link is within the browsing data page footer with ID
      // `kQuickDeleteBrowsingDataFooterIdentifier`.
      grey_ancestor(
          grey_accessibilityID(kQuickDeleteBrowsingDataFooterIdentifier)),
      grey_accessibilityLabel(@"sign out of Chrome"),
      // UIKit instantiates a `UIAccessibilityLinkSubelement` for the link
      // element in the label with attributed string.
      grey_kindOfClassName(@"UIAccessibilityLinkSubelement"),
      grey_accessibilityTrait(UIAccessibilityTraitLink), nil);
}

// Asserts if the Privacy.DeleteBrowsingData.Dialog histogram for bucket of
// `action` was logged once.
void ExpectDeleteBrowsingDataDialogHistogram(
    DeleteBrowsingDataDialogAction action) {
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(action)
          forHistogram:base::SysUTF8ToNSString(
                           browsing_data::kDeleteBrowsingDataDialogHistogram)],
      @"Privacy.DeleteBrowsingData.Dialog histogram for action %d was not "
      @"logged.",
      static_cast<int>(action));
}

// Asserts if the Privacy.DeleteBrowsingData.Dialog histogram for bucket of
// `action` was not logged.
void NoDeleteBrowsingDataDialogHistogram(
    DeleteBrowsingDataDialogAction action) {
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:0
             forBucket:static_cast<int>(action)
          forHistogram:base::SysUTF8ToNSString(
                           browsing_data::kDeleteBrowsingDataDialogHistogram)],
      @"Privacy.DeleteBrowsingData.Dialog histogram for action %d was logged.",
      static_cast<int>(action));
}

}  // namespace

// Tests the Quick Delete Browsing Data page.
@interface QuickDeleteBrowsingDataTestCase : ChromeTestCase
@end

@implementation QuickDeleteBrowsingDataTestCase

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey resetBrowsingDataPrefs];
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
}

- (void)tearDown {
  [ChromeEarlGrey resetBrowsingDataPrefs];
  // Close any open UI to avoid test flakiness.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  config.features_enabled.push_back(kIOSQuickDelete);
  config.additional_args.push_back(std::string("--") +
                                   syncer::kSyncShortNudgeDelayForTest);
  return config;
}

// Opens Quick Delete browsing data page.
- (void)openQuickDeleteBrowsingDataPage {
  [ChromeEarlGreyUI openToolsMenu];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                   l10n_util::GetNSString(
                                       IDS_IOS_TOOLS_MENU_CLEAR_BROWSING_DATA))]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:BrowsingDataButtonMatcher()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      quickDeleteBrowsingDataPageTitleMatcher()];
}

// Opens Quick Delete from the three dot menu for the specified window.
- (void)openQuickDeleteBrowsingDataPageInWindowWithNumber:(int)windowNumber {
  [ChromeEarlGreyUI openToolsMenu];

  // There is a known bug that EG fails on the second window due to a false
  // negativity visibility computation. Therefore, using the function below
  // solves that issue.
  chrome_test_util::TapAtOffsetOf(
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_CLEAR_BROWSING_DATA),
      windowNumber, CGVectorMake(0.0, 0.0));

  [[EarlGrey selectElementWithMatcher:BrowsingDataButtonMatcher()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      quickDeleteBrowsingDataPageTitleMatcher()];
}

- (void)signIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
}

// Tests the cancel button dismisses the browsing data page.
- (void)testPageNavigationCancelButton {
  // At the beginning of the test, the Delete Browsing Data dialog metric should
  // be empty.
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kBrowsingDataSelected);
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kCancelDataTypesSelected);

  // Open quick delete browsing data page.
  [self openQuickDeleteBrowsingDataPage];

  // Tap cancel button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Ensure the browsing data page is closed while quick delete bottom sheet is
  // still open.
  [[EarlGrey selectElementWithMatcher:quickDeleteBrowsingDataPageTitleMatcher()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Assert that the Delete Browsing Data dialog metric is populated.
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kBrowsingDataSelected);
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kCancelDataTypesSelected);
}

// Tests the confirm button dismisses the browsing data page.
- (void)testPageNavigationConfirmButton {
  // At the beginning of the test, the Delete Browsing Data dialog metrics
  // should be empty.
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kBrowsingDataSelected);
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kUpdateDataTypesSelected);

  // Open quick delete browsing data page.
  [self openQuickDeleteBrowsingDataPage];

  // Tap confirm button.
  [[EarlGrey selectElementWithMatcher:BrowsingDataConfirmButtonMatcher()]
      performAction:grey_tap()];

  // Ensure the page is closed while quick delete bottom sheet is still open.
  [[EarlGrey selectElementWithMatcher:quickDeleteBrowsingDataPageTitleMatcher()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Assert that the Delete Browsing Data dialog metrics are populated.
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kBrowsingDataSelected);
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kUpdateDataTypesSelected);
}

// Tests that the confirm button is disabled if no browsing data type is
// selected.
- (void)testDisabledConfirmButtonWhenNoSelection {
  // Disable selection of all browsing data types.
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:browsing_data::prefs::kDeleteBrowsingHistory];
  [ChromeEarlGrey setBoolValue:NO forUserPref:browsing_data::prefs::kCloseTabs];
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:browsing_data::prefs::kDeleteCookies];
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:browsing_data::prefs::kDeleteCache];
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:browsing_data::prefs::kDeletePasswords];
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:browsing_data::prefs::kDeleteFormData];

  [self openQuickDeleteBrowsingDataPage];

  // Check that the confirm button is disabled.
  [[EarlGrey selectElementWithMatcher:BrowsingDataConfirmButtonMatcher()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Select a browsing data type.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      performAction:grey_tap()];

  // Check that the confirm button is enabled.
  [[EarlGrey selectElementWithMatcher:BrowsingDataConfirmButtonMatcher()]
      assertWithMatcher:grey_enabled()];
}

// Tests the cancel button does not save changes to prefs.
- (void)testCancelButtonDoesNotUpdatePrefs {
  // Set all prefs to false.
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:browsing_data::prefs::kDeleteBrowsingHistory];
  [ChromeEarlGrey setBoolValue:NO forUserPref:browsing_data::prefs::kCloseTabs];
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:browsing_data::prefs::kDeleteCookies];
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:browsing_data::prefs::kDeleteCache];
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:browsing_data::prefs::kDeletePasswords];
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:browsing_data::prefs::kDeleteFormData];

  // Open quick delete browsing data page.
  [self openQuickDeleteBrowsingDataPage];

  // Assert all browsing data rows are not selected.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:tabsCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:ClearAutofillButton()]
      assertWithMatcher:elementIsSelectedMatcher(false)];

  // Tap on the browsing data cells to toggle the selection.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:tabsCellMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearAutofillButton()]
      performAction:grey_tap()];

  // Assert all browsing data rows are selected.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:tabsCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:ClearAutofillButton()]
      assertWithMatcher:elementIsSelectedMatcher(true)];

  // Tap cancel button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Ensure the browsing data page is closed while quick delete bottom sheet is
  // still open.
  [[EarlGrey selectElementWithMatcher:quickDeleteBrowsingDataPageTitleMatcher()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Assert that the pref remains false on cancel.
  GREYAssertEqual(
      [ChromeEarlGrey
          userBooleanPref:browsing_data::prefs::kDeleteBrowsingHistory],
      NO, @"History pref changed on cancel.");
  GREYAssertEqual(
      [ChromeEarlGrey userBooleanPref:browsing_data::prefs::kCloseTabs], NO,
      @"Tabs pref changed on cancel.");
  GREYAssertEqual(
      [ChromeEarlGrey userBooleanPref:browsing_data::prefs::kDeleteCookies], NO,
      @"Site data pref changed on cancel.");
  GREYAssertEqual(
      [ChromeEarlGrey userBooleanPref:browsing_data::prefs::kDeleteCache], NO,
      @"Cache pref changed on cancel.");
  GREYAssertEqual(
      [ChromeEarlGrey userBooleanPref:browsing_data::prefs::kDeletePasswords],
      NO, @"Passwords pref changed on cancel.");
  GREYAssertEqual(
      [ChromeEarlGrey userBooleanPref:browsing_data::prefs::kDeleteFormData],
      NO, @"Autofill pref changed on cancel.");

  // Check that the Delete Browsing Data dialog metric is empty, since the
  // selection wasn't confirmed.
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kBrowsingHistoryToggledOn);
}

// Tests the confirm button should save changes to prefs, in this case, from
// unselected to selected.
- (void)testConfirmButtonShouldUpdatePrefs {
  // Set all prefs to false.
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:browsing_data::prefs::kDeleteBrowsingHistory];
  [ChromeEarlGrey setBoolValue:NO forUserPref:browsing_data::prefs::kCloseTabs];
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:browsing_data::prefs::kDeleteCookies];
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:browsing_data::prefs::kDeleteCache];
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:browsing_data::prefs::kDeletePasswords];
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:browsing_data::prefs::kDeleteFormData];

  // Open quick delete browsing data page.
  [self openQuickDeleteBrowsingDataPage];

  // At the beginning of the test, the Delete Browsing Data dialog metric should
  // be empty.
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kBrowsingHistoryToggledOn);
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kTabsToggledOn);
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kSiteDataToggledOn);
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kCacheToggledOn);
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kPasswordsToggledOn);
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kAutofillToggledOn);

  // Assert all browsing data rows are not selected.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:tabsCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:ClearAutofillButton()]
      assertWithMatcher:elementIsSelectedMatcher(false)];

  // Tap on the browsing data cells to toggle the selection.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:tabsCellMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearAutofillButton()]
      performAction:grey_tap()];

  // Assert all browsing data rows are selected.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:tabsCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:ClearAutofillButton()]
      assertWithMatcher:elementIsSelectedMatcher(true)];

  // Tap confirm button.
  [[EarlGrey selectElementWithMatcher:BrowsingDataConfirmButtonMatcher()]
      performAction:grey_tap()];

  // Ensure the browsing data page is closed while quick delete bottom sheet is
  // still open.
  [[EarlGrey selectElementWithMatcher:quickDeleteBrowsingDataPageTitleMatcher()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Assert that the pref was updated to true on confirm.
  GREYAssertEqual(
      [ChromeEarlGrey
          userBooleanPref:browsing_data::prefs::kDeleteBrowsingHistory],
      YES, @"Failed to save history pref change on confirm.");
  GREYAssertEqual(
      [ChromeEarlGrey userBooleanPref:browsing_data::prefs::kCloseTabs], YES,
      @"Failed to save tabs pref change on confirm.");
  GREYAssertEqual(
      [ChromeEarlGrey userBooleanPref:browsing_data::prefs::kDeleteCookies],
      YES, @"Failed to save site data pref change on confirm.");
  GREYAssertEqual(
      [ChromeEarlGrey userBooleanPref:browsing_data::prefs::kDeleteCache], YES,
      @"Failed to save cache pref change on confirm.");
  GREYAssertEqual(
      [ChromeEarlGrey userBooleanPref:browsing_data::prefs::kDeletePasswords],
      YES, @"Failed to save passwords pref change on confirm.");
  GREYAssertEqual(
      [ChromeEarlGrey userBooleanPref:browsing_data::prefs::kDeleteFormData],
      YES, @"Failed to save autofill pref change on confirm.");

  // Assert that the Delete Browsing Data dialog metric is populated.
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kBrowsingHistoryToggledOn);
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kTabsToggledOn);
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kSiteDataToggledOn);
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kCacheToggledOn);
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kPasswordsToggledOn);
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kAutofillToggledOn);
}

// Tests the "sign out of Chrome" link in the footer.
- (void)testSignOutFooterLink {
  // Sign in is required to show the footer.
  [self signIn];

  // Open quick delete browsing data page.
  [self openQuickDeleteBrowsingDataPage];

  // Check that the footer is presented.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kQuickDeleteBrowsingDataFooterIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // At the beginning of the test, the buckets should be empty.
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:0
             forBucket:static_cast<int>(
                           signin_metrics::ProfileSignout::
                               kUserClickedSignoutFromClearBrowsingDataPage)
          forHistogram:@"Signin.SignoutProfile"],
      @"Signin.SignoutProfile histogram is logged at the start of the test.");
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kSignoutLinkOpened);

  // Tap on the "sign out of Chrome" link.
  // As the sign out link can be split into two lines we need a more precise
  // point tap to avoid failure on larger screens.
  [[EarlGrey selectElementWithMatcher:SignOutLinkMatcher()]
      performAction:chrome_test_util::TapAtPointPercentage(0.95, 0.05)];

  // Dismiss the sign outsnackbar, so that it can't obstruct other UI items.
  [SigninEarlGreyUI dismissSignoutSnackbar];

  // Assert that the footer is hidden.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kQuickDeleteBrowsingDataFooterIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Assert that the user is signed out.
  [SigninEarlGrey verifySignedOut];

  // Assert that the correct sign out metrics are populated.
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(
                           signin_metrics::ProfileSignout::
                               kUserClickedSignoutFromClearBrowsingDataPage)
          forHistogram:@"Signin.SignoutProfile"],
      @"Signin.SignoutProfile histogram not logged.");
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kSignoutLinkOpened);
}

- (void)testSelectionUpdateInMultiwindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  // Set history pref to false.
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:browsing_data::prefs::kDeleteBrowsingHistory];
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // At the beginning of the test, the Delete Browsing Data dialog metric should
  // be empty.
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kBrowsingHistoryToggledOn);

  // Focus the first window for the subsequent interactions.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(0)];
  // Open browsing data page in the first window.
  [self openQuickDeleteBrowsingDataPageInWindowWithNumber:0];

  // Focus the second window for the subsequent interactions.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(1)];
  // Open browsing data page in the second window.
  [self openQuickDeleteBrowsingDataPageInWindowWithNumber:1];

  // Assert history row is not selected in the second window.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      assertWithMatcher:elementIsSelectedMatcher(false)];

  // Focus the first window for the subsequent interactions.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(0)];
  // Assert history row is not selected in the first window.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      assertWithMatcher:elementIsSelectedMatcher(false)];

  // Tap on the history cell to toggle the selection on the first window.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      performAction:grey_tap()];

  // Assert history row is selected in the first window.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      assertWithMatcher:elementIsSelectedMatcher(true)];

  // Focus the first window for the subsequent interactions.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(1)];

  // Assert history row remains not selected on the second window.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      assertWithMatcher:elementIsSelectedMatcher(false)];

  // Focus the first window for the subsequent interactions.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(0)];

  // Tap confirm button on the first window where the history cell is selected.
  [[EarlGrey selectElementWithMatcher:BrowsingDataConfirmButtonMatcher()]
      performAction:grey_tap()];

  // Focus the second window for the subsequent interactions.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(1)];

  // Assert history row is selected in the second window after the pref update.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      assertWithMatcher:elementIsSelectedMatcher(true)];

  // Assert that the Delete Browsing Data dialog metric is populated only once,
  // when the selection is saved.
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kBrowsingHistoryToggledOn);
}

@end
