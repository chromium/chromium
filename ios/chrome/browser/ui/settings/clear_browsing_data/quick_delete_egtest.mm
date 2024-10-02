// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/browsing_data/core/cookie_or_cache_deletion_choice.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/command_line_switches.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using browsing_data::DeleteBrowsingDataDialogAction;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ClearBrowsingDataButton;
using chrome_test_util::ClearBrowsingDataView;
using chrome_test_util::ContainsPartialText;
using chrome_test_util::ContextMenuItemWithAccessibilityLabel;
using chrome_test_util::CreateTabGroupCreateButton;
using chrome_test_util::RecentTabsDestinationButton;
using chrome_test_util::SettingsMenuPrivacyButton;
using chrome_test_util::TabGridCellAtIndex;
using chrome_test_util::TabGridGroupCellAtIndex;
using chrome_test_util::TabGroupCreationView;

// Constant for timeout while waiting for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(10);

// GURL inserted into the history service to mock history entries.
const GURL mockURL("http://not-a-real-site.test/");

// Link for my activity page.
const char kMyActivityURL[] = "myactivity.google.com";

// Creates a group with default title from a tab cell at index `tab_cell_index`
// when no group is in the grid.
void CreateDefaultGroupFromTabCellAtIndex(int tab_cell_index) {
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(tab_cell_index)]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:
          ContextMenuItemWithAccessibilityLabel(l10n_util::GetPluralNSStringF(
              IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP, 1))]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGroupCreationView()];
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButton()]
      performAction:grey_tap()];
}

// Identifier for cell at given `index` in the tab grid.
NSString* IdentifierForTabCellAtIndex(unsigned int index) {
  return [NSString stringWithFormat:@"%@%u", kGridCellIdentifierPrefix, index];
}

// Matcher for the tab cell at the given `index`.
id<GREYMatcher> TabCellMatcherAtIndex(unsigned int index) {
  return grey_allOf(grey_accessibilityID(IdentifierForTabCellAtIndex(index)),
                    grey_kindOfClassName(@"GridCell"),
                    grey_sufficientlyVisible(), nil);
}

// Opens the tab group at `group_cell_index`.
void OpenTabGroupAtIndex(int group_cell_index) {
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGridGroupCellAtIndex(
                                                          group_cell_index)];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(group_cell_index)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabCellMatcherAtIndex(0)];
}

// Returns a matcher for the text in the browsing data summary corresponding to
// cache.
id<GREYMatcher> BrowsingDataSummaryWithCache() {
  return ContainsPartialText(l10n_util::GetNSString(
      IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_CACHED_FILES));
}

// Returns a matcher for the row with the `timeRange` on the popup menu.
id<GREYMatcher> PopupCellMenuItemWithTimeRange(NSString* timeRange) {
  return grey_allOf(
      grey_not(grey_accessibilityID(kQuickDeletePopUpButtonIdentifier)),
      ContextMenuItemWithAccessibilityLabel(timeRange), nil);
}

// Returns a matcher for the actual button with the `timeRange` inside the time
// range popup row.
id<GREYMatcher> PopupCellWithTimeRange(NSString* timeRange) {
  return grey_allOf(grey_accessibilityID(kQuickDeletePopUpButtonIdentifier),
                    grey_text(timeRange), nil);
}

// Matcher for Search history link in the footer.
id<GREYMatcher> SearchHistoryLink() {
  return grey_allOf(
      // The link is within the security footer with ID
      // `kQuickDeleteFooterIdentifier`.
      grey_ancestor(grey_accessibilityID(kQuickDeleteFooterIdentifier)),
      grey_accessibilityLabel(@"Search history"),
      // UIKit instantiates a `UIAccessibilityLinkSubelement` for the link
      // element in the label with attributed string.
      grey_kindOfClassName(@"UIAccessibilityLinkSubelement"),
      grey_accessibilityTrait(UIAccessibilityTraitLink), nil);
}

// Matcher for other forms of activity link in footer.
id<GREYMatcher> OtherFormsOfActivityLink() {
  return grey_allOf(
      // The link is within the security footer with ID
      // `kQuickDeleteFooterIdentifier`.
      grey_ancestor(grey_accessibilityID(kQuickDeleteFooterIdentifier)),
      grey_accessibilityLabel(@"other forms of activity"),
      // UIKit instantiates a `UIAccessibilityLinkSubelement` for the link
      // element in the label with attributed string.
      grey_kindOfClassName(@"UIAccessibilityLinkSubelement"),
      grey_accessibilityTrait(UIAccessibilityTraitLink), nil);
}

// Expects my activity histogram entries for `navigation`.
void ExpectClearBrowsingDataNavigationHistograms(
    MyActivityNavigation navigation) {
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(navigation)
          forHistogram:@"Settings.ClearBrowsingData.OpenMyActivity"],
      @"Settings.ClearBrowsingData.OpenMyActivity histogram not logged.");
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

// Asserts if the History.ClearBrowsingData.UserDeletedCookieOrCacheFromDialog
// histogram for bucket of `choice` was logged once.
void ExpectClearBrowsingDataCookieOrCacheDeletedHistogram(
    browsing_data::CookieOrCacheDeletionChoice choice) {
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(choice)
          forHistogram:
              @"History.ClearBrowsingData.UserDeletedCookieOrCacheFromDialog"],
      @"History.ClearBrowsingData.UserDeletedCookieOrCacheFromDialog for "
      @"choice %d histogram not logged.",
      choice);
}

// Returns the given `string` with the first letter capitalized.
NSString* CapitalizeFirstLetter(NSString* string) {
  return [[[string substringToIndex:1] uppercaseString]
      stringByAppendingString:[string substringFromIndex:1]];
}

}  // namespace

// Tests the Quick Delete UI, the new version of Delete Browsing Data.
@interface QuickDeleteTestCase : ChromeTestCase
@end

@implementation QuickDeleteTestCase

- (void)setUp {
  [super setUp];

  // Ensure that inactive tabs preference settings is set to its default state.
  [ChromeEarlGrey setIntegerValue:0
                forLocalStatePref:prefs::kInactiveTabsTimeThreshold];
  GREYAssertEqual(
      0,
      [ChromeEarlGrey localStateIntegerPref:prefs::kInactiveTabsTimeThreshold],
      @"Inactive tabs preference is not set to default value.");

  [AutofillAppInterface clearCreditCardStore];
  [PasswordSettingsAppInterface clearPasswordStores];
  [ChromeEarlGrey clearBrowsingHistory];
  [ChromeEarlGrey resetBrowsingDataPrefs];

  // Disable tab selection so the tab closure animation is not ran in all the
  // tests.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kCloseTabs];

  if (![self isRunningTest:@selector(testInactiveTabsForDeletion)]) {
    GREYAssertNil([MetricsAppInterface setupHistogramTester],
                  @"Cannot setup histogram tester.");
    [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
  }
}

- (void)tearDown {
  // Ensure that inactive tabs preference settings is set to its default state.
  [ChromeEarlGrey setIntegerValue:0
                forLocalStatePref:prefs::kInactiveTabsTimeThreshold];
  GREYAssertEqual(
      0,
      [ChromeEarlGrey localStateIntegerPref:prefs::kInactiveTabsTimeThreshold],
      @"Inactive tabs preference is not set to default value.");

  [AutofillAppInterface clearCreditCardStore];
  [PasswordSettingsAppInterface clearPasswordStores];
  [ChromeEarlGrey clearBrowsingHistory];
  [ChromeEarlGrey resetBrowsingDataPrefs];

  // Reenable the tab selection so it goes back to the default state.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kCloseTabs];

  if (![self isRunningTest:@selector(testInactiveTabsForDeletion)]) {
    [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
    GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                  @"Cannot reset histogram tester.");
  }

  // Shutdown network process after tests run to avoid hanging from
  // deleting browsing history.
  [ChromeEarlGrey killWebKitNetworkProcess];

  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  config.features_enabled.push_back(kIOSQuickDelete);
  config.additional_args.push_back(std::string("--") +
                                   syncer::kSyncShortNudgeDelayForTest);
  config.features_enabled.push_back(kTabGroupsIPad);
  config.features_enabled.push_back(kModernTabStrip);
  return config;
}

// Relaunches the app with Inactive Tabs still enabled.
- (void)relaunchAppWithInactiveTabsEnabled {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled.push_back(kIOSQuickDelete);
  config.additional_args.push_back(
      "--enable-features=" + std::string(kTabInactivityThreshold.name) + ":" +
      kTabInactivityThresholdParameterName + "/" +
      kTabInactivityThresholdImmediateDemoParam);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

// Opens Quick Delete from the Privacy page in Settings.
- (void)openQuickDeleteFromPrivacySettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];

  [ChromeEarlGreyUI
      tapPrivacyMenuButton:ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                               IDS_IOS_CLEAR_BROWSING_DATA_TITLE))];
}

// Opens Quick Delete from the three dot menu.
- (void)openQuickDeleteFromThreeDotMenu {
  [ChromeEarlGreyUI openToolsMenu];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                   l10n_util::GetNSString(
                                       IDS_IOS_TOOLS_MENU_CLEAR_BROWSING_DATA))]
      performAction:grey_tap()];

  // Wait for the summary to be loaded.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_text(l10n_util::GetNSString(
                          IDS_CLEAR_BROWSING_DATA_CALCULATING))];
}

// Opens Quick Delete from the three dot menu.
- (void)openQuickDeleteFromThreeDotMenu:(int)windowIndex {
  [ChromeEarlGreyUI openToolsMenu];

  // There is a known bug that EG fails on the second window due to a false
  // negativity visibility computation. Therefore, using the function below
  // solves that issue.
  chrome_test_util::TapAtOffsetOf(
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_CLEAR_BROWSING_DATA),
      windowIndex, CGVectorMake(0.0, 0.0));

  // Wait for the summary to be loaded.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_text(l10n_util::GetNSString(
                          IDS_CLEAR_BROWSING_DATA_CALCULATING))];
}

// Triggers the deletion from Quick Delete by tapping the delete data button and
// waits for the deletion to be finished.
- (void)triggerDeletionFromQuickDelete {
  [ChromeEarlGreyUI tapClearBrowsingDataMenuButton:ClearBrowsingDataButton()];

  // Wait for Quick Delete to disappear marking that the deletion has finished.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:ClearBrowsingDataView()];

  // Wait for the tabs closure animation to finish if it's trigger by the
  // deletion.
  [ChromeEarlGreyUI waitForAppToIdle];
}

- (void)signIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
}

- (void)signInAndEnableHistorySync {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableHistorySync:YES];

  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
}

// Opens Quick Delete from the History page.
- (void)openQuickDeleteFromHistory {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::HistoryDestinationButton()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          HistoryClearBrowsingDataButton()]
      performAction:grey_tap()];
}

// Tests if the Quick Delete UI is shown correctly from Privacy settings.
- (void)testOpenAndDismissQuickDeleteFromPrivacySettings {
  // At the beginning of the test, the Delete Browsing Data dialog metric should
  // be empty.
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kPrivacyEntryPointSelected);

  [self openQuickDeleteFromPrivacySettings];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Swipe the bottom sheet down.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that Quick Delete has been dismissed.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_nil()];

  // Check that the privacy table is in view.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_SETTINGS_PRIVACY_TITLE))]
      assertWithMatcher:grey_notNil()];

  // Assert that the Delete Browsing Data dialog metric is populated.
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kPrivacyEntryPointSelected);
}

// Tests if the Quick Delete UI is shown correctly from the three dot menu entry
// point and if dismissing implicitly by swipping down works correctly.
- (void)testOpenAndDismissImplicityQuickDeleteFromThreeDotMenu {
  // At the beginning of the test, the Delete Browsing Data dialog metrics
  // should be empty.
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kDialogDismissedImplicitly);
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kMenuItemEntryPointSelected);

  // Open Quick Delete.
  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Swipe the bottom sheet down.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that Quick Delete has been dismissed.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_nil()];

  // Assert that the Delete Browsing Data dialog metrics are populated.
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kDialogDismissedImplicitly);
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kMenuItemEntryPointSelected);
}

// Tests if the Quick Delete UI is shown correctly from the three dot menu entry
// point and if tapping cancel works correctly.
- (void)testOpenAndCancelQuickDeleteFromThreeDotMenu {
  [self openQuickDeleteFromThreeDotMenu];

  // At the beginning of the test, the Delete Browsing Data dialog metric should
  // be empty.
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kCancelSelected);

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Tap the cancel button.
  [ChromeEarlGreyUI tapClearBrowsingDataMenuButton:
                        ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                            IDS_IOS_DELETE_BROWSING_DATA_CANCEL))];

  // Check that Quick Delete has been dismissed.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_nil()];

  // Assert that the Delete Browsing Data dialog metric is populated.
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kCancelSelected);
}

// Tests if the Quick Delete UI is shown correctly from the history entry point.
- (void)testOpenAndDismissQuickDeleteFromHistory {
  // At the beginning of the test, the Delete Browsing Data dialog metric should
  // be empty.
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kHistoryEntryPointSelected);

  // Open Quick Delete.
  [self openQuickDeleteFromHistory];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Swipe the bottom sheet down.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that Quick Delete has been dismissed.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_nil()];

  // Assert that the Delete Browsing Data dialog metric is populated.
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kHistoryEntryPointSelected);
}

// Tests that the browsing data button is disabled if no browsing data type is
// selected.
- (void)testDisabledBrowsingDataButtonWhenNoSelection {
  // Disable selection of all browsing data types.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteBrowsingHistory];
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kCloseTabs];
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteCookies];
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteCache];
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeletePasswords];
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteFormData];

  [self openQuickDeleteFromThreeDotMenu];

  id<GREYMatcher> browsingDataButton = ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_DELETE_BROWSING_DATA_BUTTON));
  // Check that the browsing data button is disabled.
  [[EarlGrey selectElementWithMatcher:browsingDataButton]
      assertWithMatcher:grey_not(grey_enabled())];

  // Select a browsing data type.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kDeleteBrowsingHistory];

  // Check that the browsing data button is enabled.
  [[EarlGrey selectElementWithMatcher:browsingDataButton]
      assertWithMatcher:grey_enabled()];
}

// Tests that the time range first shows the pref value but only updates the
// pref when the user goes through with the deletion.
- (void)testTimeRangeForDeletionSelection {
  // Set pref to the last hour.
  [ChromeEarlGrey
      setIntegerValue:static_cast<int>(browsing_data::TimePeriod::LAST_HOUR)
          forUserPref:browsing_data::prefs::kDeleteTimePeriod];

  [self openQuickDeleteFromThreeDotMenu];

  // At the beginning of the test, the Delete Browsing Data dialog metric should
  // be empty.
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kLast15MinutesSelected);

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the time range row is presented and the correct time range, last
  // hour, is selected.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 PopupCellWithTimeRange(l10n_util::GetNSString(
                     IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_HOUR))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the time range button.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TITLE))]
      performAction:grey_tap()];

  // Tap on the last 15 minutes option on the popup menu.
  [[EarlGrey
      selectElementWithMatcher:
          PopupCellMenuItemWithTimeRange(l10n_util::GetNSString(
              IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_LAST_15_MINUTES))]
      performAction:grey_tap()];

  // Make sure the menu was dismissed after the tap.
  [[EarlGrey
      selectElementWithMatcher:
          PopupCellMenuItemWithTimeRange(l10n_util::GetNSString(
              IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_LAST_15_MINUTES))]
      assertWithMatcher:grey_notVisible()];

  // Check that the cell has changed to the correct selection, i.e. is showing
  // the last 15 minutes time range.
  [[EarlGrey
      selectElementWithMatcher:
          PopupCellWithTimeRange(l10n_util::GetNSString(
              IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_LAST_15_MINUTES))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Confirm that the pref has not been changed.
  GREYAssertEqual(
      [ChromeEarlGrey userIntegerPref:browsing_data::prefs::kDeleteTimePeriod],
      static_cast<int>(browsing_data::TimePeriod::LAST_HOUR),
      @"Incorrect local pref value.");

  // Tap the browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Confirm that only after the user has gone through with the deletion, the
  // pref has been saved with the new value of last 15 minutes.
  GREYAssertEqual(
      [ChromeEarlGrey userIntegerPref:browsing_data::prefs::kDeleteTimePeriod],
      static_cast<int>(browsing_data::TimePeriod::LAST_15_MINUTES),
      @"Incorrect local pref value.");

  // Assert that the Delete Browsing Data dialog metric is populated.
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kLast15MinutesSelected);
}

// Tests that the number of browsing history items is shown on the browsing data
// row when browsing history is selected as a data type to be deleted. It also
// tests that the history entries get deleted when the deletion of browsing data
// is selected.
- (void)testBrowsingHistoryForDeletion {
  // Add entry to the history service.
  [ChromeEarlGrey addHistoryServiceTypedURL:mockURL];

  // Set pref to select deletion of browsing history.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kDeleteBrowsingHistory];

  [self openQuickDeleteFromThreeDotMenu];

  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kDeletionSelected);
  GREYAssertNil([MetricsAppInterface
                     expectCount:0
                       forBucket:static_cast<int>(
                                     browsing_data::DeleteBrowsingDataAction::
                                         kClearBrowsingDataDialog)
                    forHistogram:@"Privacy.DeleteBrowsingData.Action"],
                @"Privacy.DeleteBrowsingData.Action histogram was logged.");

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row and the browsing history substring are
  // presented.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetPluralNSStringF(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITES, 1))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Check that the history entry was deleted.
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 0,
                  @"History entries were not deleted.");

  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kDeletionSelected);
  GREYAssertNil([MetricsAppInterface
                     expectCount:1
                       forBucket:static_cast<int>(
                                     browsing_data::DeleteBrowsingDataAction::
                                         kClearBrowsingDataDialog)
                    forHistogram:@"Privacy.DeleteBrowsingData.Action"],
                @"Privacy.DeleteBrowsingData.Action histogram was not logged.");
}

// Tests that the number of browsing history items is shown on the browsing data
// row when browsing history is selected as a data type to be deleted and when
// the user syncs history. It also tests that the history entries get deleted
// when the deletion of browsing data is selected.
- (void)testBrowsingHistoryForDeletionWithHistorySync {
  // Sign in and enable history sync.
  [self signInAndEnableHistorySync];

  // Add entry to the history service and wait for it to show up on the server.
  [ChromeEarlGrey addHistoryServiceTypedURL:mockURL];
  [ChromeEarlGrey waitForSyncServerHistoryURLs:@[ net::NSURLWithGURL(mockURL) ]
                                       timeout:kSyncOperationTimeout];

  // Set pref to select deletion of browsing history.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kDeleteBrowsingHistory];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row and the browsing history substring are
  // presented.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetPluralNSStringF(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITES_SYNCED, 1))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Check that the history entry was deleted.
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 0,
                  @"History entries were not deleted.");
}

// Tests that the number of browsing history items is not shown on the browsing
// data row when browsing history is not selected as a data type to be deleted.
// It also tests that the history entries do not get deleted when the deletion
// of browsing data is selected.
- (void)testKeepBrowsingHistory {
  // Add entry to the history service.
  [ChromeEarlGrey addHistoryServiceTypedURL:mockURL];

  // Set pref to keep browsing history.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteBrowsingHistory];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row is present but that the browsing history
  // substring is not.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetPluralNSStringF(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITES, 1))]
      assertWithMatcher:grey_nil()];

  // Tap the browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Check that the history entry was not deleted.
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 1,
                  @"History entries were deleted.");
}

// Tests that tabs are shown as a possible type to be deleted on the browsing
// data row when tabs are selected as a data type for deletion. It also tests
// that the tabs get closed when the deletion of tabs is selected.
- (void)testTabsForDeletion {
  // Set pref to close tabs.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kCloseTabs];

  // Load page in tab.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  [self openQuickDeleteFromThreeDotMenu];

  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kDeletionSelected);
  GREYAssertNil([MetricsAppInterface
                     expectCount:0
                       forBucket:static_cast<int>(
                                     browsing_data::DeleteBrowsingDataAction::
                                         kClearBrowsingDataDialog)
                    forHistogram:@"Privacy.DeleteBrowsingData.Action"],
                @"Privacy.DeleteBrowsingData.Action histogram was logged.");

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row and the tabs substring are presented.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetPluralNSStringF(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_TABS, 1))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Check that the tab has been closed.
  [ChromeEarlGrey waitForWebStateNotContainingText:"Echo"];
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 0, @"Tabs were not closed.");

  // Check that tab grid is shown. Quick Delete was opened from the three dot
  // menu, and as such the animation should be triggered and the tab grid should
  // be visible by the end.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridNewTabButton()]
      performAction:grey_tap()];

  // Go to Recent Tabs.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:RecentTabsDestinationButton()];

  // Check that the tabs closed through Quick Delete are not shown in Recent
  // Tabs.
  id<GREYMatcher> recentTabsTable = grey_accessibilityID(
      kRecentTabsTableViewControllerAccessibilityIdentifier);
  id<GREYMatcher> titleOfPage =
      grey_allOf(grey_ancestor(recentTabsTable),
                 chrome_test_util::StaticTextWithAccessibilityLabel(@"Echo"),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:titleOfPage]
      assertWithMatcher:grey_nil()];

  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kDeletionSelected);
  GREYAssertNil([MetricsAppInterface
                     expectCount:1
                       forBucket:static_cast<int>(
                                     browsing_data::DeleteBrowsingDataAction::
                                         kClearBrowsingDataDialog)
                    forHistogram:@"Privacy.DeleteBrowsingData.Action"],
                @"Privacy.DeleteBrowsingData.Action histogram was not logged.");
}

// Tests that when Quick Delete is opened from Privacy Settings, from an iPhone
// and tabs are selected as a data type, that the privacy settings are still
// visible after the deletion. This test is only for iPhones. on iPads the
// animation is still run.
- (void)testTabsForDeletionFromPrivacySettingsForIphones {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPads. In iPads, the tabs clsoure animation is ran even "
        @"if Quick Delete is triggered on top of privacy settings.");
  }

  // Set pref to close tabs.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kCloseTabs];

  // Load page in tab.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  [self openQuickDeleteFromPrivacySettings];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Tap the browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Check that the tab has been closed.
  [ChromeEarlGrey waitForWebStateNotContainingText:"Echo"];
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 0, @"Tabs were not closed.");

  // Quick Delete was opened from privacy settings, and as such no animation
  // should be triggered, and the privacy settings should still be visible by
  // the end.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_SETTINGS_PRIVACY_TITLE))]
      assertWithMatcher:grey_notNil()];
}

// Tests that when Quick Delete is opened from Privacy Settings, from an iPad
// and tabs are selected as a data type, that the privacy settings are not
// visible after the deletion. This test is only for iPads. on iPhones the
// animation is not run.
- (void)testTabsForDeletionFromPrivacySettingsForiPads {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPhones. In iPhones, the tabs clsoure animation is not "
        @"run if Quick Delete is triggered on top of privacy settings.");
  }

  // Set pref to close tabs.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kCloseTabs];

  // Load page in tab.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  [self openQuickDeleteFromPrivacySettings];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Tap the browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Check that the tab has been closed.
  [ChromeEarlGrey waitForWebStateNotContainingText:"Echo"];
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 0, @"Tabs were not closed.");

  // Check that the privacy settings are not opened, since the animation was
  // run.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_text(l10n_util::GetNSString(IDS_IOS_SETTINGS_PRIVACY_TITLE))];
}

// Tests that inactive tabs are shown as a possible type to be deleted on the
// browsing data row when tabs are selected as a data type for deletion. It also
// tests that the inactive tabs get closed when the deletion of tabs is
// selected.
- (void)testInactiveTabsForDeletion {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }

  // Set pref to close tabs.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kCloseTabs];

  // Load page in tab.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tab count should be 0");

  // Relaunch the app to create the inactive tab. Relaunces also creates a new
  // NTP tab.
  [self relaunchAppWithInactiveTabsEnabled];

  // Load a url in the NTP tab.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 1,
                 @"Inactive tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1, @"Tab count should be 1");

  // Open Quick Delete.
  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row and the tabs substring are presented.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetPluralNSStringF(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_TABS, 2))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Check that the tabs have been closed.
  [ChromeEarlGrey waitForWebStateNotContainingText:"Echo"];
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tabs were not closed.");
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 0, @"Tabs were not closed.");
}

// Tests that tabs in tab groups are shown as a possible type to be deleted on
// the browsing data row when tabs are selected as a data type for deletion. It
// also tests that the tabs in tab groups get closed when the deletion of tabs
// is selected.
- (void)testTabsForDeletionInTabGroup {
  // Set pref to close tabs.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kCloseTabs];

  // Load page in tab.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  // Open tab grid and create a tab group.
  [ChromeEarlGreyUI openTabGrid];
  CreateDefaultGroupFromTabCellAtIndex(0);
  OpenTabGroupAtIndex(0);

  // Open tab is in the group.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      performAction:grey_tap()];

  // Open Quick Delete.
  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row and the tabs substring are presented.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ContainsPartialText(@"1 tab,")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Check that the tab has been closed.
  [ChromeEarlGrey waitForWebStateNotContainingText:"Echo"];
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 0, @"Tabs were not closed.");
}

// Tests that tabs are shown as a possible type to be deleted on the browsing
// data row when tabs are selected as a data type for deletion. The number of
// tabs should include tabs in all windows, not just the ones where quick delete
// is triggered from. It also tests that the tabs in both windows get closed
// when the deletion of tabs is selected.
- (void)testTabsForDeletionInMultiwindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  // Set pref to close tabs.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kCloseTabs];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Load page in first window.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  // Open page in second window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  // In the first window, open quick delete and check that the browsing data row
  // and the browsing tabs substring are presented.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(0)];
  [self openQuickDeleteFromThreeDotMenu];
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetPluralNSStringF(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_TABS, 2))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the delete browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Check that the tabs have been closed in both windows.
  [ChromeEarlGrey waitForWebStateNotContainingText:"Echo"];
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(0)];
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 0, @"Tabs were not closed.");
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(1)];
  [ChromeEarlGrey waitForWebStateNotContainingText:"Echo"];
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 0, @"Tabs were not closed.");
}

// Tests that the selected value for the time range updates across all open
// Quick Delete menus.
- (void)testTimeRangeSelectionUpdatesInMultiwindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  // Set time range pref to the last hour.
  [ChromeEarlGrey
      setIntegerValue:static_cast<int>(browsing_data::TimePeriod::LAST_HOUR)
          forUserPref:browsing_data::prefs::kDeleteTimePeriod];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Open a new window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // At the beginning of the test, the Delete Browsing Data dialog metric should
  // be empty.
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kLast15MinutesSelected);

  // In the first window, open quick delete and check that time range is set to
  // the last hour.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(0)];

  // Open Quick Delete menu.
  [self openQuickDeleteFromThreeDotMenu:0];

  // Assess that time range is set to the last hour.
  [[EarlGrey selectElementWithMatcher:
                 PopupCellWithTimeRange(l10n_util::GetNSString(
                     IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_HOUR))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // In the second window, open quick delete and check that time range is set to
  // the last hour.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(1)];

  // Open Quick Delete menu.
  [self openQuickDeleteFromThreeDotMenu:1];

  // Assess that time range is set to the last hour.
  [[EarlGrey selectElementWithMatcher:
                 PopupCellWithTimeRange(l10n_util::GetNSString(
                     IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_HOUR))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Focus on the first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(0)];

  // Open time range popup menu.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TITLE))]
      performAction:grey_tap()];

  // Tap on the last 15 minutes option on the popup menu.
  [[EarlGrey
      selectElementWithMatcher:
          PopupCellMenuItemWithTimeRange(l10n_util::GetNSString(
              IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_LAST_15_MINUTES))]
      performAction:grey_tap()];

  // Check that the cell has changed to the correct selection, i.e. is showing
  // the last 15 minutes time range.
  [[EarlGrey
      selectElementWithMatcher:
          PopupCellWithTimeRange(l10n_util::GetNSString(
              IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_LAST_15_MINUTES))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the browsing data button so the time range pref is saved.
  [self triggerDeletionFromQuickDelete];

  // Focus on the second window.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(1)];

  // Assess that the time range is also set to the last 15 minutes.
  [[EarlGrey
      selectElementWithMatcher:
          PopupCellWithTimeRange(l10n_util::GetNSString(
              IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_LAST_15_MINUTES))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Assert that the Delete Browsing Data dialog metric is populated.
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kLast15MinutesSelected);
}

// Tests that the number of tabs are not shown on the browsing data row when
// tabs are not selected as a data type to be deleted. It also tests that the
// tabs do not get closed when the deletion of tabs is not selected.
- (void)testKeepTabs {
  // Load page in tab.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  // Set pref to close tabs.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kCloseTabs];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row is present but that the tabs substring is
  // not.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetPluralNSStringF(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_TABS, 1))]
      assertWithMatcher:grey_nil()];

  // Tap the browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Check that the tab has not been closed.
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1, @"Tabs were closed.");
}

// Tests that cookies are shown as a possible type to be deleted on the browsing
// data row when cookies are selected as a data type for deletion.
- (void)testCookiesForDeletion {
  // Set pref to select deletion of cookies.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kDeleteCookies];
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteCache];
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kCloseTabs];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row and the cookie substring are presented.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          ContainsPartialText(CapitalizeFirstLetter(l10n_util::GetNSString(
              IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITE_DATA)))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Assert that the Delete Browsing Data dialog metric is populated.
  ExpectClearBrowsingDataCookieOrCacheDeletedHistogram(
      browsing_data::CookieOrCacheDeletionChoice::kOnlyCookies);
}

// Tests that cookies are not shown as a possible type to be deleted on the
// browsing data row when cookies are not selected as a data type for deletion.
- (void)testKeepCookies {
  // Set pref to keep cookies.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteCookies];
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteCache];
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kCloseTabs];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row is present but that the cookie substring
  // is not.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          ContainsPartialText(CapitalizeFirstLetter(l10n_util::GetNSString(
              IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITE_DATA)))]
      assertWithMatcher:grey_nil()];
}

// Tests that cache is shown as a possible type to be deleted on the browsing
// data row when cache is selected as a data type for deletion.
- (void)testCacheForDeletion {
  // Set pref to select deletion of cache.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kDeleteCache];
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteCookies];
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kCloseTabs];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row and the cached substring are presented.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          ContainsPartialText(CapitalizeFirstLetter(l10n_util::GetNSString(
              IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_CACHED_FILES)))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Assert that the Delete Browsing Data dialog metric is populated.
  ExpectClearBrowsingDataCookieOrCacheDeletedHistogram(
      browsing_data::CookieOrCacheDeletionChoice::kOnlyCache);
}

// Tests that cache is not shown as a possible type to be deleted on the
// browsing data row when cache is not selected as a data type for deletion.
- (void)testKeepCache {
  // Set pref to keep cache.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteCache];
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteCookies];
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kCloseTabs];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row is presented but that the cached substring
  // is not.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          ContainsPartialText(CapitalizeFirstLetter(l10n_util::GetNSString(
              IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_CACHED_FILES)))]
      assertWithMatcher:grey_nil()];
}

// Tests that the number of passwords is shown on the browsing data row if
// passwords is selected as a data type to be deleted. It also tests the
// password gets deleted when the deletion of browsing data is selected.
- (void)testPasswordsForDeletion {
  // Add password to password autofill store.
  int kPasswordCount = 1;
  [PasswordSettingsAppInterface
      saveExamplePasswordToProfileWithCount:kPasswordCount];

  // Set pref to select deletion of passwords.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kDeletePasswords];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row and the passwords substring are presented.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:
          ContainsPartialText(l10n_util::GetPluralNSStringF(
              IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PASSWORDS, kPasswordCount))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Check that the stored password was removed.
  GREYAssertEqual(
      0, [PasswordSettingsAppInterface passwordProfileStoreResultsCount],
      @"Stored password was not removed.");
}

// Tests that the number of passwords is not shown on the browsing data row if
// passwords is not selected as a data type to be deleted. It also tests that
// the password does not get deleted when the deletion of browsing data is
// selected.
- (void)testKeepPasswords {
  // Save a card to the payments data manager.
  int kPasswordCount = 1;
  [PasswordSettingsAppInterface
      saveExamplePasswordToProfileWithCount:kPasswordCount];

  // Set pref to keep passwords.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeletePasswords];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row is present but that the passwords
  // substring is not.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetPluralNSStringF(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PASSWORDS,
                     kPasswordCount))] assertWithMatcher:grey_nil()];

  // Tap the browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Check that the stored password was not removed.
  GREYAssertEqual(
      1, [PasswordSettingsAppInterface passwordProfileStoreResultsCount],
      @"Stored password was removed.");
}

// Tests that the number of payment methods is shown on the browsing data row if
// form data is selected as a data type to be deleted. It also tests that the
// cards gets deleted when the deletion of browsing data is selected.
- (void)testPaymentMethodsForDeletion {
  // Save a card to the payments data manager.
  [AutofillAppInterface saveLocalCreditCard];

  // Set pref to select deletion of form data.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kDeleteFormData];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row and the form data substring are presented.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetPluralNSStringF(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PAYMENT_METHODS, 1))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Check that the stored card was removed.
  GREYAssertEqual(0, [AutofillAppInterface localCreditCount],
                  @"Stored card was not removed.");
}

// Tests that the number of cards is not shown on the browsing data row if form
// data is not selected as a data type to be deleted. It also tests that the
// cards does not get deleted when the deletion of browsing data is selected.
- (void)testKeepPaymentMethods {
  // Save a card.
  [AutofillAppInterface saveLocalCreditCard];

  // Set pref to keep form data.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteFormData];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row is presented but that the form data
  // substring is not.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetPluralNSStringF(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PAYMENT_METHODS, 1))]
      assertWithMatcher:grey_nil()];

  // Tap the browsing data button.
  [self triggerDeletionFromQuickDelete];

  // Check that the stored card was not removed.
  GREYAssertEqual(1, [AutofillAppInterface localCreditCount],
                  @"Stored card was removed.");
}

// Tests that if there is no data available for deletion of the selected data
// types, that the placeholder summary for no data is shown.
- (void)testNoDataForDeletion {
  // Make sure there isn't any history items.
  [ChromeEarlGrey clearBrowsingHistory];

  // Set pref to keep browsing history.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteBrowsingHistory];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row is presented with the placeholder summary
  // for no data.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_NO_DATA))]
      assertWithMatcher:grey_nil()];
}

// Tests the footer search history link is opened correctly and metrics are
// recorded in the corrresponding histogram bucket.
- (void)testOpenSearchHistoryMyActivityFooterLink {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  // Sign in is required to show the footer.
  [self signIn];
  // Open Quick Delete bottom sheet.
  [self openQuickDeleteFromThreeDotMenu];

  // At the beginning of the test, the Delete Browsing Data dialog metric should
  // be empty.
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kSearchHistoryLinkOpened);

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the footer is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kQuickDeleteFooterIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the "Search history" link.
  [[EarlGrey selectElementWithMatcher:SearchHistoryLink()]
      performAction:grey_tap()];

  // Check that my activity link was opened.
  GREYAssertEqual(std::string(kMyActivityURL),
                  [ChromeEarlGrey webStateVisibleURL].host(),
                  @"Did not navigate to the search activity url.");

  // Assert that the metrics are populated.
  ExpectClearBrowsingDataNavigationHistograms(
      MyActivityNavigation::kSearchHistory);
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kSearchHistoryLinkOpened);
}

// Tests the footer other forms of activity link is opened correctly and metrics
// are recorded in the corrresponding histogram bucket.
- (void)testOpenOtherFormsOfActivityMyActivityFooterLink {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  // Sign in is required to show the footer.
  [self signIn];
  // Open Quick Delete bottom sheet.
  [self openQuickDeleteFromThreeDotMenu];

  // At the beginning of the test, the Delete Browsing Data dialog metric should
  // be empty.
  NoDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kMyActivityLinkedOpened);

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the footer is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kQuickDeleteFooterIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the "Search history" link.
  [[EarlGrey selectElementWithMatcher:OtherFormsOfActivityLink()]
      performAction:grey_tap()];

  // Check that my activity link was opened.
  GREYAssertEqual(std::string(kMyActivityURL),
                  [ChromeEarlGrey webStateVisibleURL].host(),
                  @"Did not navigate to the search activity url.");

  // Assert that the metrics are populated.
  ExpectClearBrowsingDataNavigationHistograms(MyActivityNavigation::kTopLevel);
  ExpectDeleteBrowsingDataDialogHistogram(
      DeleteBrowsingDataDialogAction::kMyActivityLinkedOpened);
}

// Tests the footer discalimer string is hidden when the user is signed out and
// shown when the user signs in.
- (void)testHideShowFooterBasedOnSignInStatus {
  // Open Quick Delete bottom sheet.
  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the footer is hidden.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kQuickDeleteFooterIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Swipe the bottom sheet down.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that Quick Delete has been dismissed.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_nil()];

  // Sign in to the browser.
  [self signIn];

  // Re-open Quick Delete bottom sheet.
  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Check that the footer is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kQuickDeleteFooterIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that a user in the `ConsentLevel::kSignin` state will remain signed in
// after clearing their browsing history.
- (void)testUserSignedInWhenClearingBrowsingData {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Open Quick Delete and delete browsing data.
  [self openQuickDeleteFromThreeDotMenu];
  [self triggerDeletionFromQuickDelete];

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that a supervised user in the `ConsentLevel::kSync` state will remain
// signed-in after clearing their browsing history.
// TODO(crbug.com/40066949): Delete this test after the syncing state is gone.
- (void)testSupervisedUserSyncingWhenClearingBrowsingData {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kIsSubjectToParentalControlsCapabilityName) : @YES,
                 }];
  [SigninEarlGrey signinAndEnableLegacySyncFeature:fakeIdentity];

  // Open Quick Delete and delete browsing data.
  [self openQuickDeleteFromThreeDotMenu];
  [self triggerDeletionFromQuickDelete];

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that a supervised user in the `ConsentLevel::kSignin` state will remain
// signed-in after clearing their browsing history.
- (void)testSupervisedUserSignedInWhenClearingBrowsingData {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kIsSubjectToParentalControlsCapabilityName) : @YES,
                 }];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Open Quick Delete and delete browsing data.
  [self openQuickDeleteFromThreeDotMenu];
  [self triggerDeletionFromQuickDelete];

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that changing the state of one pref, updates the browsing data summary
// across all open Quick Delete menus.
- (void)testPrefChangeUpdatesInMultiwindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  // Set the cache preference to true.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kDeleteCache];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Open a new window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // In the first window, open quick delete and check that browsing data summary
  // contains cache related information.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(0)];

  // Open Quick Delete menu.
  [self openQuickDeleteFromThreeDotMenu:0];

  // Assess that the browsing data summary contains the "cache" keyword.
  [[EarlGrey selectElementWithMatcher:BrowsingDataSummaryWithCache()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // In the second window, open quick delete and check that browsing data
  // summary contains cache related information.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(1)];

  // Open Quick Delete menu.
  [self openQuickDeleteFromThreeDotMenu:1];

  // Assess that the browsing data summary contains the "cache" keyword.
  [[EarlGrey selectElementWithMatcher:BrowsingDataSummaryWithCache()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Set the cache preference to false.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteCache];

  // Focus on the first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(0)];

  // Assess that the summary is updated on the first page (i.e. cache pref is no
  // longer displayed in the summary).
  [[EarlGrey selectElementWithMatcher:BrowsingDataSummaryWithCache()]
      assertWithMatcher:grey_nil()];

  // Focus on the second window.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(1)];

  // Assess that the cache pref is no longer displayed in the summary on the
  // second window.
  [[EarlGrey selectElementWithMatcher:BrowsingDataSummaryWithCache()]
      assertWithMatcher:grey_nil()];
}

@end
