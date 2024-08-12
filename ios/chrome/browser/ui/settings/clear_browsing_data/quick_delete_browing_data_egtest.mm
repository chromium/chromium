// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/test/metrics/histogram_tester.h"
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
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using chrome_test_util::ButtonWithAccessibilityLabel;

// Returns a matcher for the title of the Quick Delete bottom sheet.
id<GREYMatcher> quickDeleteTitleMatcher() {
  return grey_allOf(
      grey_accessibilityID(kConfirmationAlertTitleAccessibilityIdentifier),
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE)),
      nil);
}

// Returns a matcher for the Quick Delete Browsing Data button on the main page.
id<GREYMatcher> quickDeleteBrowsingDataButtonMatcher() {
  return grey_accessibilityID(kQuickDeleteBrowsingDataButtonIdentifier);
}

// Returns a matcher for the title of the Quick Delete Browsing Data page.
id<GREYMatcher> quickDeleteBrowsingDataPageTitleMatcher() {
  return chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
      IDS_IOS_DELETE_BROWSING_DATA_TITLE);
}

// Returns a matcher for the confirm button on the navigation bar.
id<GREYMatcher> navigationBarConfirmButtonMatcher() {
  return grey_accessibilityID(kQuickDeleteBrowsingDataConfirmButtonIdentifier);
}

// Returns matcher for an element with or without the
// UIAccessibilityTraitSelected accessibility trait depending on `selected`.
id<GREYMatcher> elementIsSelectedMatcher(bool selected) {
  return selected
             ? grey_accessibilityTrait(UIAccessibilityTraitSelected)
             : grey_not(grey_accessibilityTrait(UIAccessibilityTraitSelected));
}

// Returns a matcher for the passwords cell.
id<GREYMatcher> historyCellMatcher() {
  return grey_allOf(
      grey_accessibilityID(kQuickDeleteBrowsingDataHistoryIdentifier),
      grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the tabs cell.
id<GREYMatcher> tabsCellMatcher() {
  return grey_allOf(
      grey_accessibilityID(kQuickDeleteBrowsingDataTabsIdentifier),
      grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the site data cell.
id<GREYMatcher> siteDataCellMatcher() {
  return grey_allOf(
      grey_accessibilityID(kQuickDeleteBrowsingDataSiteDataIdentifier),
      grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the cache cell.
id<GREYMatcher> cacheCellMatcher() {
  return grey_allOf(
      grey_accessibilityID(kQuickDeleteBrowsingDataCacheIdentifier),
      grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the passwords cell.
id<GREYMatcher> passwordsCellMatcher() {
  return grey_allOf(
      grey_accessibilityID(kQuickDeleteBrowsingDataPasswordsIdentifier),
      grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the autofill cell.
id<GREYMatcher> autofillCellMatcher() {
  return grey_allOf(
      grey_accessibilityID(kQuickDeleteBrowsingDataAutofillIdentifier),
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
                                       IDS_IOS_CLEAR_BROWSING_DATA_TITLE))]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:quickDeleteBrowsingDataButtonMatcher()]
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
  [[EarlGrey selectElementWithMatcher:quickDeleteTitleMatcher()]
      assertWithMatcher:grey_notNil()];
}

// Tests the confirm button dismisses the browsing data page.
- (void)testPageNavigationConfirmButton {
  // Open quick delete browsing data page.
  [self openQuickDeleteBrowsingDataPage];

  // Tap confirm button.
  [[EarlGrey selectElementWithMatcher:navigationBarConfirmButtonMatcher()]
      performAction:grey_tap()];

  // Ensure the page is closed while quick delete bottom sheet is still open.
  [[EarlGrey selectElementWithMatcher:quickDeleteBrowsingDataPageTitleMatcher()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:quickDeleteTitleMatcher()]
      assertWithMatcher:grey_notNil()];
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
  [[EarlGrey selectElementWithMatcher:historyCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:tabsCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:siteDataCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:cacheCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:passwordsCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:autofillCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];

  // Tap on the browsing data cells to toggle the selection.
  [[EarlGrey selectElementWithMatcher:historyCellMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:tabsCellMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:siteDataCellMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:cacheCellMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:passwordsCellMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:autofillCellMatcher()]
      performAction:grey_tap()];

  // Assert all browsing data rows are selected.
  [[EarlGrey selectElementWithMatcher:historyCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:tabsCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:siteDataCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:cacheCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:passwordsCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:autofillCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(true)];

  // Tap cancel button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Ensure the browsing data page is closed while quick delete bottom sheet is
  // still open.
  [[EarlGrey selectElementWithMatcher:quickDeleteBrowsingDataPageTitleMatcher()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:quickDeleteTitleMatcher()]
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
}

// Tests the confirm button should save changes to prefs.
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

  // Assert all browsing data rows are not selected.
  [[EarlGrey selectElementWithMatcher:historyCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:tabsCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:siteDataCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:cacheCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:passwordsCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:autofillCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];

  // Tap on the browsing data cells to toggle the selection.
  [[EarlGrey selectElementWithMatcher:historyCellMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:tabsCellMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:siteDataCellMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:cacheCellMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:passwordsCellMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:autofillCellMatcher()]
      performAction:grey_tap()];

  // Assert all browsing data rows are selected.
  [[EarlGrey selectElementWithMatcher:historyCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:tabsCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:siteDataCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:cacheCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:passwordsCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:autofillCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(true)];

  // Tap confirm button.
  [[EarlGrey selectElementWithMatcher:navigationBarConfirmButtonMatcher()]
      performAction:grey_tap()];

  // Ensure the browsing data page is closed while quick delete bottom sheet is
  // still open.
  [[EarlGrey selectElementWithMatcher:quickDeleteBrowsingDataPageTitleMatcher()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:quickDeleteTitleMatcher()]
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

  // At the beginning of the test this bucket should be empty.
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:0
             forBucket:static_cast<int>(
                           signin_metrics::ProfileSignout::
                               kUserClickedSignoutFromClearBrowsingDataPage)
          forHistogram:@"Signin.SignoutProfile"],
      @"Signin.SignoutProfile histogram is logged at the start of the test.");

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

  // Assert that the correct sign out metrics bucket is populated.
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(
                           signin_metrics::ProfileSignout::
                               kUserClickedSignoutFromClearBrowsingDataPage)
          forHistogram:@"Signin.SignoutProfile"],
      @"Signin.SignoutProfile histogram not logged.");
}

@end
