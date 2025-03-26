// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/sync/base/command_line_switches.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/history/ui_bundled/history_ui_constants.h"
#import "ios/chrome/browser/menu/ui_bundled/menu_action_type.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/popup_menu_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_coordinator_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::DeleteButton;
using chrome_test_util::HistoryClearBrowsingDataButton;
using chrome_test_util::HistoryEntry;
using chrome_test_util::OpenLinkInIncognitoButton;
using chrome_test_util::OpenLinkInNewTabButton;
using chrome_test_util::OpenLinkInNewWindowButton;

namespace {
char kURL1[] = "http://example.org/firstURL";
char kURL2[] = "http://example.org/secondURL";
char kURL3[] = "http://example.org/thirdURL";
char kTitle1[] = "Page 1";
char kTitle2[] = "Page 2";

// Matcher for the edit button in the navigation bar.
id<GREYMatcher> NavigationEditButton() {
  return grey_accessibilityID(kHistoryToolbarEditButtonIdentifier);
}
// Matcher for the delete button.
id<GREYMatcher> DeleteHistoryEntriesButton() {
  return grey_accessibilityID(kHistoryToolbarDeleteButtonIdentifier);
}
// Matcher for the search button.
id<GREYMatcher> SearchIconButton() {
  return grey_accessibilityID(kHistorySearchControllerSearchBarIdentifier);
}

// Asserts if the expected `count` was recorded for
// Privacy.DeleteBrowsingData.Action.HistoryPageEntries.
void ExpectDeleteBrowsingDataHistoryHistogram(int count) {
  GREYAssertNil([MetricsAppInterface
                     expectCount:count
                       forBucket:static_cast<int>(
                                     browsing_data::DeleteBrowsingDataAction::
                                         kHistoryPageEntries)
                    forHistogram:@"Privacy.DeleteBrowsingData.Action"],
                @"Privacy.DeleteBrowsingData.Action histogram for the "
                @"HistoryPageEntries bucket "
                @"page entries did not have count %d.",
                count);
}

void ExpectContextMenuHistoryEntryActionsHistogram(int count,
                                                   MenuActionType action) {
  GREYAssertNil([MetricsAppInterface
                     expectCount:count
                       forBucket:static_cast<int>(action)
                    forHistogram:@"Mobile.ContextMenu.HistoryEntry.Actions"],
                @"Mobile.ContextMenu.HistoryEntry.Actions histogram for the "
                @"%d action "
                @"page entries did not have count %d.",
                static_cast<int>(action), count);
}

}  // namespace

// History UI tests.
@interface HistoryUITestCase : ChromeTestCase
@end

@implementation HistoryUITestCase {
  GURL _URL1;
  GURL _URL2;
  GURL _URL3;
}

- (void)setUp {
  [super setUp];

  _URL1 = GURL(kURL1);
  _URL2 = GURL(kURL2);
  _URL3 = GURL(kURL3);

  if (![ChromeTestCase forceRestartAndWipe]) {
    [ChromeEarlGrey clearBrowsingHistory];
  }

  // Some tests rely on a clean state for the "Clear Browsing Data" settings
  // screen.
  [ChromeEarlGrey resetBrowsingDataPrefs];

  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
}

- (void)tearDownHelper {
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);

  // Some tests change the default values for the "Clear Browsing Data" settings
  // screen.
  [ChromeEarlGrey resetBrowsingDataPrefs];

  [super tearDownHelper];
}

#pragma mark Tests

// Tests that no history is shown if there has been no navigation.
- (void)testDisplayNoHistory {
  [ChromeCoordinatorAppInterface startHistoryCoordinator];
  [ChromeEarlGreyUI assertHistoryHasNoEntries];
  [ChromeCoordinatorAppInterface reset];
}

// Tests that the history panel displays navigation history.
- (void)testDisplayHistory {
  [self addTestURLsToHistory];
  [ChromeCoordinatorAppInterface startHistoryCoordinator];

  // Assert that history displays three entries.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Tap a history entry and assert that navigation to that entry's URL occurs.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Assert that the tapped URL was loaded.
  GREYAssertEqualObjects(ChromeCoordinatorAppInterface.lastURLLoaded,
                         net::NSURLWithGURL(_URL1),
                         @"URL1 should have loaded.");
  [ChromeCoordinatorAppInterface reset];
}

// Tests that searching history displays only entries matching the search term.
- (void)testSearchHistory {
  [self addTestURLsToHistory];
  [ChromeCoordinatorAppInterface startHistoryCoordinator];
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];

  // Verify that scrim is visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistorySearchScrimIdentifier)]
      assertWithMatcher:grey_notNil()];

  NSString* searchString =
      [NSString stringWithFormat:@"%s", _URL1.path().c_str()];

  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(searchString)];

  // Verify that scrim is not visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistorySearchScrimIdentifier)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      assertWithMatcher:grey_nil()];
  [ChromeCoordinatorAppInterface reset];
}

// Tests that long press on scrim while search box is enabled dismisses the
// search controller.
- (void)testSearchLongPressOnScrimCancelsSearchController {
  [self addTestURLsToHistory];
  [ChromeCoordinatorAppInterface startHistoryCoordinator];
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];

  // Try long press.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_longPress()];

  // Verify context menu is not visible.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      assertWithMatcher:grey_nil()];

  // Verify that scrim is not visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistorySearchScrimIdentifier)]
      assertWithMatcher:grey_nil()];

  // Verifiy we went back to original folder content.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeCoordinatorAppInterface reset];
}

// Tests deletion of history entries.
- (void)testDeleteHistory {
  // Assert that the DeleteBrowsingData histogram is empty at the beginning of
  // the test.
  ExpectDeleteBrowsingDataHistoryHistogram(0);

  [self addTestURLsToHistory];
  [ChromeCoordinatorAppInterface startHistoryCoordinator];

  // Assert that three history elements are present.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Enter edit mode, select a history element, and press delete.
  [[EarlGrey selectElementWithMatcher:NavigationEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:DeleteHistoryEntriesButton()]
      performAction:grey_tap()];

  // Assert that the deleted entry is gone and the other two remain.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Assert that the DeleteBrowsingData histogram contains one bucket after one
  // deletion was requested.
  ExpectDeleteBrowsingDataHistoryHistogram(1);

  // Enter edit mode, select both remaining entries, and press delete.
  [[EarlGrey selectElementWithMatcher:NavigationEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:DeleteHistoryEntriesButton()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI assertHistoryHasNoEntries];

  // Assert that the DeleteBrowsingData histogram contains two bucket after the
  // second deletion was requested.
  ExpectDeleteBrowsingDataHistoryHistogram(2);

  [ChromeCoordinatorAppInterface reset];
}

// Tests that tapping the Clear Browsing Data button/link.
- (void)testClearBrowsingDataButton {
  [self addTestURLsToHistory];
  [ChromeCoordinatorAppInterface startHistoryCoordinator];
  [[EarlGrey selectElementWithMatcher:HistoryClearBrowsingDataButton()]
      performAction:grey_tap()];
  GREYAssert([ChromeCoordinatorAppInterface
                 selectorWasDispatched:
                     @"showQuickDeleteAndCanPerformTabsClosureAnimation:"],
             @"Command was not dispatched");
  [ChromeCoordinatorAppInterface reset];
}

// Tests display and selection of 'Open in New Tab' in a context menu on a
// history entry.
- (void)testContextMenuOpenInNewTab {
  // At the beginning of the test, the Context Menu History Entry Actions metric
  // should be empty.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::OpenInNewTab);

  [self addTestURLsToHistory];
  [ChromeCoordinatorAppInterface startHistoryCoordinator];

  // Long press on the history element.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_longPress()];

  // Select "Open in New Tab" and confirm that new tab is opened with selected
  // URL.
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];
  GREYAssertEqualObjects(ChromeCoordinatorAppInterface.lastURLLoaded,
                         net::NSURLWithGURL(_URL1),
                         @"URL1 should have loaded.");

  // Assert that the Context Menu History Entry Actions metric is populated.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::OpenInNewTab);
  [ChromeCoordinatorAppInterface reset];
}

// Tests display and selection of 'Open in New Window' in a context menu on a
// history entry.
- (void)testContextMenuOpenInNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  // At the beginning of the test, the Context Menu History Entry Actions metric
  // should be empty.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::OpenInNewWindow);

  [self addTestURLsToHistory];
  [ChromeCoordinatorAppInterface startHistoryCoordinator];

  // Long press on the history element.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:OpenLinkInNewWindowButton()]
      performAction:grey_tap()];
  GREYAssert([ChromeCoordinatorAppInterface
                 selectorWasDispatched:@"openNewWindowWithActivity:"],
             @"Command was not dispatched");

  // Assert that the Context Menu History Entry Actions metric is populated.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::OpenInNewWindow);
  [ChromeCoordinatorAppInterface reset];
}

// Tests display and selection of 'Open in New Incognito Tab' in a context menu
// on a history entry.
- (void)testContextMenuOpenInNewIncognitoTab {
  // At the beginning of the test, the Context Menu History Entry Actions metric
  // should be empty.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::OpenInNewIncognitoTab);

  [self addTestURLsToHistory];
  [ChromeCoordinatorAppInterface startHistoryCoordinator];

  // Long press on the history element.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_longPress()];

  // Select "Open in New Incognito Tab" and confirm that new tab is opened in
  // incognito with the selected URL.
  //  [ChromeEarlGrey verifyOpenInIncognitoActionWithURL:_URL1.GetContent()];
  [[EarlGrey selectElementWithMatcher:OpenLinkInIncognitoButton()]
      performAction:grey_tap()];
  GREYAssertEqualObjects(ChromeCoordinatorAppInterface.lastURLLoaded,
                         net::NSURLWithGURL(_URL1),
                         @"URL1 should have loaded.");
  GREYAssert(ChromeCoordinatorAppInterface.lastURLLoadedInIncognito,
             @"URL should have loaded in incognito");

  // Assert that the Context Menu History Entry Actions metric is populated.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::OpenInNewIncognitoTab);
  [ChromeCoordinatorAppInterface reset];
}

// Tests display and selection of 'Copy URL' in a context menu on a history
// entry.
- (void)testContextMenuCopy {
  // At the beginning of the test, the Context Menu History Entry Actions metric
  // should be empty.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::CopyURL);

  [self addTestURLsToHistory];
  [ChromeCoordinatorAppInterface startHistoryCoordinator];

  // Long press on the history element.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_longPress()];

  // Tap "Copy URL" and wait for the URL to be copied to the pasteboard.
  [ChromeEarlGrey
      verifyCopyLinkActionWithText:[NSString
                                       stringWithUTF8String:_URL1.spec()
                                                                .c_str()]];

  // Assert that the Context Menu History Entry Actions metric is populated.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::CopyURL);
  [ChromeCoordinatorAppInterface reset];
}

// Tests display and selection of "Share" in the context menu for a history
// entry.
- (void)testContextMenuShare {
  // At the beginning of the test, the Context Menu History Entry Actions metric
  // should be empty.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::Share);

  [self addTestURLsToHistory];
  [ChromeCoordinatorAppInterface startHistoryCoordinator];

  // Long press on the history element.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_longPress()];

  [ChromeEarlGrey
      verifyShareActionWithURL:_URL1
                     pageTitle:[NSString stringWithUTF8String:kTitle1]];

  // Assert that the Context Menu History Entry Actions metric is populated.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::Share);
  [ChromeCoordinatorAppInterface reset];
}

// Tests the Delete context menu action for a History entry.
- (void)testContextMenuDelete {
  // Assert that the DeleteBrowsingData histogram is empty at the beginning of
  // the test.
  ExpectDeleteBrowsingDataHistoryHistogram(0);
  // At the beginning of the test, the Context Menu History Entry Actions metric
  // should be empty.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::Delete);

  [self addTestURLsToHistory];
  [ChromeCoordinatorAppInterface startHistoryCoordinator];

  // Long press on the history element.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:DeleteButton()] performAction:grey_tap()];

  // Assert that the deleted entry is gone and the other two remain.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_nil()];

  // Wait for the animations to be done, then validate.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:HistoryEntry(_URL2,
                                                                kTitle2)];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Assert that the DeleteBrowsingData histogram contains one bucket after one
  // deletion was requested.
  ExpectDeleteBrowsingDataHistoryHistogram(1);
  // Assert that the Context Menu History Entry Actions metric is populated.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::Delete);
  [ChromeCoordinatorAppInterface reset];
}

// Tests that the VC can be dismissed by swiping down.
- (void)testSwipeDownDismiss {
  [self addTestURLsToHistory];
  [ChromeCoordinatorAppInterface startHistoryCoordinator];

  // Check that the TableView is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryTableViewIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryTableViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the TableView has been dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryTableViewIdentifier)]
      assertWithMatcher:grey_nil()];
  [ChromeCoordinatorAppInterface reset];
}

// Tests that the VC can be dismissed by swiping down while its searching.
- (void)testSwipeDownDismissWhileSearching {
  [self addTestURLsToHistory];
  [ChromeCoordinatorAppInterface startHistoryCoordinator];

  // Check that the TableView is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryTableViewIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Search for the first URL.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];
  NSString* searchString =
      [NSString stringWithFormat:@"%s", _URL1.path().c_str()];
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(searchString)];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryTableViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the TableView has been dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryTableViewIdentifier)]
      assertWithMatcher:grey_nil()];
  [ChromeCoordinatorAppInterface reset];
}

// Navigates to history and checks elements for accessibility.
- (void)testAccessibilityOnHistory {
  [self addTestURLsToHistory];
  [ChromeCoordinatorAppInterface startHistoryCoordinator];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      grey_accessibilityID(kHistoryTableViewIdentifier)];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [ChromeCoordinatorAppInterface reset];
}

#pragma mark Helper Methods

// Inserts three test URLs into the History Service.
- (void)addTestURLsToHistory {
  [ChromeEarlGrey addHistoryServiceTypedURL:_URL1];
  [ChromeEarlGrey setHistoryServiceTitle:kTitle1 forPage:_URL1];
  [ChromeEarlGrey addHistoryServiceTypedURL:_URL2];
  [ChromeEarlGrey setHistoryServiceTitle:kTitle2 forPage:_URL2];
  [ChromeEarlGrey addHistoryServiceTypedURL:_URL3];
}

@end
