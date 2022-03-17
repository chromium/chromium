// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/format_macros.h"
#include "base/ios/ios_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForUIElementTimeout;
using chrome_test_util::TabGridOtherDevicesPanelButton;
using chrome_test_util::LongPressCellAndDragToEdge;
using chrome_test_util::LongPressCellAndDragToOffsetOf;
using chrome_test_util::TapAtOffsetOf;
using chrome_test_util::WindowWithNumber;
using chrome_test_util::AddToBookmarksButton;
using chrome_test_util::AddToReadingListButton;
using chrome_test_util::CloseTabMenuButton;
using chrome_test_util::TabGridCellAtIndex;
using chrome_test_util::TabGridNormalModePageControl;
using chrome_test_util::TabGridSearchBar;
using chrome_test_util::TabGridSearchCancelButton;
using chrome_test_util::TabGridSearchModeToolbar;
using chrome_test_util::TabGridSearchTabsButton;
using chrome_test_util::TabGridSelectTabsMenuButton;
using chrome_test_util::RegularTabGrid;

namespace {
char kURL1[] = "http://firstURL";
char kURL2[] = "http://secondURL";
char kURL3[] = "http://thirdURL";
char kURL4[] = "http://fourthURL";
char kTitle1[] = "Page one";
char kTitle2[] = "Page two";
char kTitle4[] = "Page four";
char kResponse1[] = "Test Page 1 content";
char kResponse2[] = "Test Page 2 content";
char kResponse3[] = "Test Page 3 content";
char kResponse4[] = "Test Page 4 content";

const CFTimeInterval kSnackbarAppearanceTimeout = 5;
const CFTimeInterval kSnackbarDisappearanceTimeout = 11;

id<GREYMatcher> TabGridCell() {
  return grey_allOf(grey_kindOfClassName(@"GridCell"),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TabWithTitle(char* title) {
  return grey_allOf(
      TabGridCell(),
      grey_accessibilityLabel([NSString stringWithUTF8String:title]),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TabWithTitleAndIndex(char* title, unsigned int index) {
  return grey_allOf(TabWithTitle(title), TabGridCellAtIndex(index), nil);
}

// Identifer for cell at given |index| in the tab grid.
NSString* IdentifierForCellAtIndex(unsigned int index) {
  return [NSString stringWithFormat:@"%@%u", kGridCellIdentifierPrefix, index];
}

id<GREYMatcher> DeselectAllButton() {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                        IDS_IOS_TAB_GRID_DESELECT_ALL_BUTTON),
                    grey_userInteractionEnabled(), nullptr);
}

id<GREYMatcher> SelectAllButton() {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                        IDS_IOS_TAB_GRID_SELECT_ALL_BUTTON),
                    grey_userInteractionEnabled(), nullptr);
}

id<GREYMatcher> VisibleTabGridEditButton() {
  return grey_allOf(chrome_test_util::TabGridEditButton(),
                    grey_sufficientlyVisible(), nil);
}

void WaitForTabGridFullscreen() {
  if (![ChromeEarlGrey isThumbstripEnabledForWindowWithNumber:0]) {
    return;
  }

  // Check that the kRegularTabGridIdentifier is visible.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kRegularTabGridIdentifier)]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  bool fullscreenAchieved = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, condition);
  GREYAssertTrue(fullscreenAchieved, @"kRegularTabGridIdentifier not visible");
}

// Returns a matcher for the scrim view on the tab search.
id<GREYMatcher> SearchScrim() {
  return grey_accessibilityID(kTabGridScrimIdentifier);
}

// Returns a matcher for the search results header with title set with
// |title_id|.
id<GREYMatcher> SearchSectionHeaderWithTitleID(int title_id) {
  id<GREYMatcher> title_matcher =
      grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(title_id)),
                 grey_sufficientlyVisible(), nil);
  return grey_allOf(grey_accessibilityID(kGridSectionHeaderIdentifier),
                    grey_descendant(title_matcher), grey_sufficientlyVisible(),
                    nil);
}

// Returns a matcher for the search results open tabs section header.
id<GREYMatcher> SearchOpenTabsSectionHeader() {
  return SearchSectionHeaderWithTitleID(
      IDS_IOS_TABS_SEARCH_OPEN_TABS_SECTION_HEADER_TITLE);
}

// Returns a matcher for the search results suggested actions section header.
id<GREYMatcher> SearchSuggestedActionsSectionHeader() {
  return SearchSectionHeaderWithTitleID(IDS_IOS_TABS_SEARCH_SUGGESTED_ACTIONS);
}

// Returns a matcher for the search results open tabs section header with
// |count| set in the value label .
id<GREYMatcher> SearchOpenTabsHeaderWithValue(size_t count) {
  NSString* count_str = [NSString stringWithFormat:@"%" PRIuS, count];
  NSString* value = l10n_util::GetNSStringF(
      IDS_IOS_TABS_SEARCH_OPEN_TABS_COUNT, base::SysNSStringToUTF16(count_str));
  id<GREYMatcher> value_matcher = grey_allOf(grey_accessibilityLabel(value),
                                             grey_sufficientlyVisible(), nil);

  return grey_allOf(SearchOpenTabsSectionHeader(),
                    grey_descendant(value_matcher), grey_sufficientlyVisible(),
                    nil);
}

// Returns a matcher for the "Search on web" suggested action.
id<GREYMatcher> SearchOnWebSuggestedAction() {
  return grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabelId(
                        IDS_IOS_TABS_SEARCH_SUGGESTED_ACTION_SEARCH_WEB),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the "Search recent tabs" suggested action.
id<GREYMatcher> SearchRecentTabsSuggestedAction() {
  return grey_allOf(
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_TABS_SEARCH_SUGGESTED_ACTION_SEARCH_RECENT_TABS),
      grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the "Search history" suggested action.
id<GREYMatcher> SearchHistorySuggestedAction() {
  return grey_allOf(
      grey_accessibilityID(kTableViewTabsSearchSuggestedHistoryItemId),
      grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the "Search history (|matches_count| Found)" suggested
// action.
id<GREYMatcher> SearchHistorySuggestedActionWithMatches(size_t matches_count) {
  NSString* count_str = [NSString stringWithFormat:@"%" PRIuS, matches_count];
  NSString* history_label = l10n_util::GetNSStringF(
      IDS_IOS_TABS_SEARCH_SUGGESTED_ACTION_SEARCH_HISTORY,
      base::SysNSStringToUTF16(count_str));
  return grey_allOf(grey_accessibilityLabel(history_label),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the search suggested actions section.
id<GREYMatcher> SearchSuggestedActionsSection() {
  return grey_allOf(grey_accessibilityID(kSuggestedActionsGridCellIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the search suggested actions section with the history
// item matches count set to |matches_count|.
id<GREYMatcher> SearchSuggestedActionsSectionWithHistoryMatchesCount(
    size_t matches_count) {
  return grey_allOf(
      SearchSuggestedActionsSection(),
      grey_descendant(SearchHistorySuggestedActionWithMatches(matches_count)),
      grey_sufficientlyVisible(), nil);
}
}  // namespace

@interface TabGridTestCase : WebHttpServerChromeTestCase {
  GURL _URL1;
  GURL _URL2;
  GURL _URL3;
  GURL _URL4;
}
@end

@implementation TabGridTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  std::vector<SEL> searchTests = {
      @selector(testEnterExitSearch),
      @selector(testTabGridResetAfterExitingSearch),
      @selector(testScrimVisibleInSearchModeWhenSearchBarIsEmpty),
      @selector(testTapOnSearchScrimExitsSearchMode),
      @selector(testSearchRegularOpenTabs),
      @selector(testSearchRegularOpenTabsSelectResult),
      @selector(testSearchIncognitoOpenTabsSelectResult),
      @selector(testSearchOpenTabsContextMenuShare),
      @selector(testSearchOpenTabsContextMenuAddToReadingList),
      @selector(testSearchOpenTabsContextMenuAddToBookmarks),
      @selector(testSearchOpenTabsContextMenuCloseTab),
      @selector(testOpenTabsHeaderVisibleInSearchModeWhenSearchBarIsNotEmpty),
      @selector(testSuggestedActionsVisibleInSearchModeWhenSearchBarIsNotEmpty),
      @selector(testSearchSuggestedActionsDisplaysCorrectHistoryMatchesCount),
      @selector(testSearchSuggestedActionsSectionContentInRegularGrid),
      @selector(testSuggestedActionsNotAvailableInIncognitoPageSearchMode)};
  for (SEL test : searchTests) {
    if ([self isRunningTest:test]) {
      config.features_enabled.push_back(kTabsSearch);
      break;
    }
  }
  return config;
}

- (void)setUp {
  [super setUp];

  _URL1 = web::test::HttpServer::MakeUrl(kURL1);
  _URL2 = web::test::HttpServer::MakeUrl(kURL2);
  _URL3 = web::test::HttpServer::MakeUrl(kURL3);
  _URL4 = web::test::HttpServer::MakeUrl(kURL4);

  std::map<GURL, std::string> responses;
  const char kPageFormat[] = "<head><title>%s</title></head><body>%s</body>";
  responses[_URL1] = base::StringPrintf(kPageFormat, kTitle1, kResponse1);
  responses[_URL2] = base::StringPrintf(kPageFormat, kTitle2, kResponse2);
  // Page 3 does not have <title> tag, so URL will be its title.
  responses[_URL3] = kResponse3;
  responses[_URL4] = base::StringPrintf(kPageFormat, kTitle4, kResponse4);
  web::test::SetUpSimpleHttpServer(responses);
}

- (void)tearDown {
  [super tearDown];
  // Ensure that pref set in testTabGridItemContextMenuAddToBookmarkGreyed is
  // reset even if the test failed.
  if ([self isRunningTest:@selector
            (testTabGridItemContextMenuAddToBookmarkGreyed)]) {
    [ChromeEarlGreyAppInterface
        setBoolValue:YES
         forUserPref:base::SysUTF8ToNSString(
                         bookmarks::prefs::kEditBookmarksEnabled)];
  }
}

// Tests entering and leaving the tab grid.
- (void)testEnteringAndLeavingTabGrid {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests that tapping on the first cell shows that tab.
- (void)testTappingOnFirstCell {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that closing the cell shows no tabs, and displays the empty state.
- (void)testClosingFirstCell {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridRegularTabsEmptyStateView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tapping Close All shows no tabs, shows Undo button, and displays
// the empty state. Then tests tapping Undo shows Close All button again.
// Validates this case when Tab Grid Bulk Actions feature is enabled.
- (void)testCloseAllAndUndoCloseAll {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  // Close all tabs
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridEditMenuCloseAllButton()]
      performAction:grey_tap()];

  // Ensure tabs were closed
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_nil()];

  // Ensure undo button is visible and edit button is not visible
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridUndoCloseAllButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridRegularTabsEmptyStateView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Undo button
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridUndoCloseAllButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the Undo button is no longer available after tapping Close All,
// then creating a new tab, then coming back to the tab grid.
// Validates this case when Tab Grid Bulk Actions feature is enabled.
- (void)testUndoCloseAllNotAvailableAfterNewTabCreation {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridEditMenuCloseAllButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridUndoCloseAllButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Create a new tab then come back to tab grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridNewTabButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  // Undo is no longer available.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridUndoCloseAllButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that Clear Browsing Data can be successfully done from tab grid.
- (void)DISABLED_testClearBrowsingData {
  // Load history
  [self loadTestURLs];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  // Switch over to Recent Tabs.
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];

  // Tap on "Show History"
  // Undo is available after close all action.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectShowHistoryCell()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI openAndClearBrowsingDataFromHistory];
  [ChromeEarlGreyUI assertHistoryHasNoEntries];
}

#pragma mark - Recent Tabs Context Menu

// Tests the Copy Link action on a recent tab's context menu.
- (void)testRecentTabsContextMenuCopyLink {
  [self prepareRecentTabWithURL:_URL1 response:kResponse1];
  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  [ChromeEarlGrey
      verifyCopyLinkActionWithText:[NSString
                                       stringWithUTF8String:_URL1.spec()
                                                                .c_str()]];
}

// Tests the Open in New Tab action on a recent tab's context menu.
- (void)testRecentTabsContextMenuOpenInNewTab {
  [self prepareRecentTabWithURL:_URL1 response:kResponse1];
  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  [ChromeEarlGrey verifyOpenInNewTabActionWithURL:_URL1.GetContent()];

  // Verify that the Tab Grid is closed.
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      assertWithMatcher:grey_notVisible()];
}

// Tests the Open in New Window action on a recent tab's context menu.
- (void)testRecentTabsContextMenuOpenInNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  [self prepareRecentTabWithURL:_URL1 response:kResponse1];
  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  [ChromeEarlGrey verifyOpenInNewWindowActionWithContent:kResponse1];
}

// Tests the Share action on a recent tab's context menu.
- (void)testRecentTabsContextMenuShare {
  [self prepareRecentTabWithURL:_URL1 response:kResponse1];
  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  [ChromeEarlGrey
      verifyShareActionWithURL:_URL1
                     pageTitle:[NSString stringWithUTF8String:kTitle1]];
}

#pragma mark - Tab Grid Item Context Menu

// Tests the Share action on a tab grid item's context menu.
- (void)testTabGridItemContextMenuShare {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  [ChromeEarlGrey
      verifyShareActionWithURL:_URL1
                     pageTitle:[NSString stringWithUTF8String:kTitle1]];
}

// Tests the Add to Reading list action on a tab grid item's context menu.
- (void)testTabGridItemContextMenuAddToReadingList {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  [self waitForSnackBarMessage:IDS_IOS_READING_LIST_SNACKBAR_MESSAGE
      triggeredByTappingItemWithMatcher:AddToReadingListButton()];
}

// Tests the Add to Bookmarks action on a tab grid item's context menu.
- (void)testTabGridItemContextMenuAddToBookmarks {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  [self waitForSnackBarMessage:IDS_IOS_BOOKMARK_PAGE_SAVED
      triggeredByTappingItemWithMatcher:AddToBookmarksButton()];

  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_TOOLS_MENU_EDIT_BOOKMARK)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
                     IDS_IOS_BOOKMARK_EDIT_SCREEN_TITLE)]
      assertWithMatcher:grey_notNil()];
}

// Tests that Add to Bookmarks action is greyed out when editBookmarksEnabled
// pref is set to false.
- (void)testTabGridItemContextMenuAddToBookmarkGreyed {
  [ChromeEarlGreyAppInterface
      setBoolValue:NO
       forUserPref:base::SysUTF8ToNSString(
                       bookmarks::prefs::kEditBookmarksEnabled)];

  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];
  [[EarlGrey selectElementWithMatcher:AddToBookmarksButton()]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [ChromeEarlGreyAppInterface
      setBoolValue:YES
       forUserPref:base::SysUTF8ToNSString(
                       bookmarks::prefs::kEditBookmarksEnabled)];
}

// Tests the Share action on a tab grid item's context menu.
- (void)testTabGridItemContextCloseTab {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  // Close Tab.
  [[EarlGrey selectElementWithMatcher:CloseTabMenuButton()]
      performAction:grey_tap()];

  // Make sure that the tab is no longer present.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTitle1)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridRegularTabsEmptyStateView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - Drag and drop in Multiwindow

// Tests that dragging a tab grid item to the edge opens a new window and that
// the tab is properly transferred, incuding navigation stack.
- (void)testDragAndDropAtEdgeToCreateNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");

// TODO(crbug.com/1184267): Test is flaky on iPad devices.
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"This test is flaky on iPad devices.");
  }
#endif

  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];
  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:0];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  GREYWaitForAppToIdle(@"App failed to idle");

  GREYAssert(LongPressCellAndDragToEdge(IdentifierForCellAtIndex(0),
                                        kGREYContentEdgeRight, 0),
             @"Failed to DND cell");

  GREYWaitForAppToIdle(@"App failed to idle");

  // Assert two windows and the expected tabs in each.
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2
                             inWindowWithNumber:0];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:1];

  // Navigate back on second window to check the navigation stack is intact.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1
                             inWindowWithNumber:1];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests that dragging a tab grid incognito item to the edge opens a new window
// and that the tab is properly transferred, incuding navigation stack.
- (void)testIncognitoDragAndDropAtEdgeToCreateNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");

  [ChromeEarlGrey closeAllNormalTabs];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];
  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey waitForIncognitoTabCount:2 inWindowWithNumber:0];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  GREYWaitForAppToIdle(@"App failed to idle");

  GREYAssert(LongPressCellAndDragToEdge(IdentifierForCellAtIndex(0),
                                        kGREYContentEdgeRight, 0),
             @"Failed to DND cell");

  GREYWaitForAppToIdle(@"App failed to idle");

  // Assert two windows and the expected tabs in each.
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:0];
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2
                             inWindowWithNumber:0];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:1];

  // Navigate back on second window to check the navigation stack is intact.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1
                             inWindowWithNumber:1];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests dragging tab grid item between windows.
- (void)testDragAndDropBetweenWindows {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");

  // Setup first window with tabs 1 and 2.
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:0];

  // Open second window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Setup second window with tabs 3 and 4.
  [ChromeEarlGrey loadURL:_URL3 inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:1];

  [ChromeEarlGrey openNewTabInWindowWithNumber:1];
  [ChromeEarlGrey loadURL:_URL4 inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse4
                             inWindowWithNumber:1];

  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:1];

  // Open tab grid in both window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  GREYWaitForAppToIdle(@"App failed to idle");

  // DnD first tab of left window to left edge of first tab in second window.
  // Note: move to left half of the destination tile, to avoid unwanted
  // scrolling that would happen closer to the left edge.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(0), 0,
                                            IdentifierForCellAtIndex(0), 1,
                                            CGVectorMake(0.4, 0.5)),
             @"Failed to DND cell on cell");

  GREYWaitForAppToIdle(@"App failed to idle");

  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:3 inWindowWithNumber:1];

  // Move third cell of second window as second cell in first window.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(2), 1,
                                            IdentifierForCellAtIndex(0), 0,
                                            CGVectorMake(1.0, 0.5)),
             @"Failed to DND cell on cell");

  GREYWaitForAppToIdle(@"App failed to idle");

  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:1];

  // Check content and order of tabs.
  [self fromGridCheckTabAtIndex:0 inWindowNumber:0 containsText:kResponse2];
  [self fromGridCheckTabAtIndex:1 inWindowNumber:0 containsText:kResponse4];
  [self fromGridCheckTabAtIndex:0 inWindowNumber:1 containsText:kResponse1];
  [self fromGridCheckTabAtIndex:1 inWindowNumber:1 containsText:kResponse3];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests dragging incognito tab grid item between windows.
- (void)testDragAndDropIncognitoBetweenWindows {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");

  // Setup first window with one incognito tab.
  [ChromeEarlGrey closeAllNormalTabs];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:0];
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:0];

  // Open second window with main ntp.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];

  // Open tab grid in both window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  GREYWaitForAppToIdle(@"App failed to idle");

  // Try DnDing first incognito tab of left window to main tab panel on right
  // window.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(0), 0,
                                            IdentifierForCellAtIndex(0), 1,
                                            CGVectorMake(1.0, 0.5)),
             @"Failed to DND cell on cell");

  GREYWaitForAppToIdle(@"App failed to idle");

  // It should fail and both windows should still have only one tab.
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];

  // Move second window to incognito tab panel.
  // Note: until reported bug is fixed in EarlGrey, grey_tap() doesn't always
  // work in second window, because it fails the visibility check.
  GREYAssert(TapAtOffsetOf(kTabGridIncognitoTabsPageButtonIdentifier, 1,
                           CGVectorMake(0.5, 0.5)),
             @"Failed to tap incognito panel button");

  // Try again to move tabs.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(0), 0, nil,
                                            1, CGVectorMake(0.5, 0.5)),
             @"Failed to DND cell on window");

  GREYWaitForAppToIdle(@"App failed to idle");

  // Check that it worked and there are 2 incgnito tabs in second window.
  [ChromeEarlGrey waitForIncognitoTabCount:0 inWindowWithNumber:0];
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:1];

  // Cleanup.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests dragging tab grid item as URL between windows.
- (void)testDragAndDropURLBetweenWindows {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");

  // Setup first window with tabs 1 and 2.
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:0];

  // Open second window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Setup second window with tab 3.
  [ChromeEarlGrey loadURL:_URL3 inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:1];

  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];

  // Open tab grid in first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  GREYWaitForAppToIdle(@"App failed to idle");

  // DnD first tab of left window to second window.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(0), 0, nil,
                                            1, CGVectorMake(0.5, 0.5)),
             @"Failed to DND cell on window");

  GREYWaitForAppToIdle(@"App failed to idle");

  // Tabs should not have changed.
  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];

  // Second window should show URL1
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1
                             inWindowWithNumber:1];

  // Navigate back to check the navigation stack is intact.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:1];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests dragging tab grid incognito item as URL to a main windows.
- (void)testDragAndDropIncognitoURLInMainWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");

  // Setup first window with one incognito tab 1.
  [ChromeEarlGrey closeAllNormalTabs];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:0];
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:0];

  // Open second window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Setup second window with tab 3.
  [ChromeEarlGrey loadURL:_URL3 inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:1];

  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];

  // Open incognito tab grid in first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];

  GREYWaitForAppToIdle(@"App failed to idle");

  // DnD first tab of left window to second window.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(0), 0, nil,
                                            1, CGVectorMake(0.5, 0.5)),
             @"Failed to DND cell on window");

  GREYWaitForAppToIdle(@"App failed to idle");

  // Tabs should not have changed.
  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:0];
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];

  // Second window should show URL1
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1
                             inWindowWithNumber:1];

  // Navigate back to check the navigation stack is intact.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:1];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests dragging tab grid main item as URL to an incognito windows.
- (void)testDragAndDropMainURLInIncognitoWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");

// TODO(crbug.com/1184267): Test is flaky on iPad devices.
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"This test is flaky on iPad devices.");
  }
#endif

  // Setup first window with one incognito tab 1.
  [ChromeEarlGrey closeAllNormalTabs];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:0];
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:0];

  // Open second window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Setup second window with tab 3.
  [ChromeEarlGrey loadURL:_URL3 inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:1];

  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];

  // Open incognito tab grid in first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  GREYWaitForAppToIdle(@"App failed to idle");

  // DnD first tab of second window to first window.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(0), 1, nil,
                                            0, CGVectorMake(0.5, 0.5)),
             @"Failed to DND cell on window");

  GREYWaitForAppToIdle(@"App failed to idle");

  // Tabs should not have changed.
  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:0];
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];

  // First window should show URL3
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:0];

  // Navigate back to check the navigation stack is intact.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1
                             inWindowWithNumber:0];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

#pragma mark - Bulk Actions

// Tests closing a tab in the tab grid edit mode and that edit mode is exited
// after closing all tabs.
- (void)testTabGridBulkActionCloseTabs {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];
  WaitForTabGridFullscreen();

  // Tap tab to select.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridEditCloseTabsButton()]
      performAction:grey_tap()];

  NSString* closeTabsButtonText =
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_TAB_GRID_CLOSE_ALL_TABS_CONFIRMATION,
          /*number=*/1));

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                   closeTabsButtonText)]
      performAction:grey_tap()];

  // Make sure that the tab is no longer present.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTitle1)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridRegularTabsEmptyStateView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify edit mode is exited.
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      assertWithMatcher:grey_notNil()];
}

// Tests selecting all items in the tab grid edit mode using the "Select all"
// button.
- (void)testTabGridBulkActionSelectAll {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];
  WaitForTabGridFullscreen();

  // Tap "Select all" and close selected tabs.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridEditSelectAllButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridEditCloseTabsButton()]
      performAction:grey_tap()];
  NSString* closeTabsButtonText =
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_TAB_GRID_CLOSE_ALL_TABS_CONFIRMATION,
          /*number=*/3));
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                   closeTabsButtonText)]
      performAction:grey_tap()];

  // Make sure that the tab grid is empty.
  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:0];

  // Verify edit mode is exited.
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      assertWithMatcher:grey_notNil()];
}

// Tests deselecting all items in the tab grid edit mode using the "Deselect
// all" button.
- (void)testTabGridBulkActionDeselectAll {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];
  WaitForTabGridFullscreen();

  // Ensure button label is "Select All" and select all items.
  [[EarlGrey selectElementWithMatcher:SelectAllButton()]
      performAction:grey_tap()];

  // Deselect all button should be visible when all items are selected.
  // Tapping deselect all button should deselect all items.
  [[EarlGrey selectElementWithMatcher:DeselectAllButton()]
      performAction:grey_tap()];

  // Verify deselection by manually tapping each item (to re-select) and closing
  // the selected items.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(1)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(2)]
      performAction:grey_tap()];

  // All tabs should have been re-selected and closing selected tabs should
  // empty the tab grid.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridEditCloseTabsButton()]
      performAction:grey_tap()];
  NSString* closeTabsButtonText =
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_TAB_GRID_CLOSE_ALL_TABS_CONFIRMATION,
          /*number=*/3));
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                   closeTabsButtonText)]
      performAction:grey_tap()];

  // Make sure that the tab grid is empty.
  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:0];
}

// Tests adding items to Bookmarks from the tab grid edit mode.
- (void)testTabGridBulkActionAddToBookmarks {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];
  WaitForTabGridFullscreen();

  // Select the first and last items.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(2)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridEditAddToButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:AddToBookmarksButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
                     IDS_IOS_BOOKMARK_CHOOSE_GROUP_BUTTON)]
      assertWithMatcher:grey_notNil()];

  // Choose "Mobile Bookmarks" folder as the destination.
  // Duplicate matcher here instead of using +[BookmarkEarlGreyUI
  // openMobileBookmarks] in order to properly wait for the snackbar message.
  NSString* snackBarMessage =
      l10n_util::GetNSStringF(IDS_IOS_BOOKMARK_PAGE_SAVED_FOLDER,
                              base::SysNSStringToUTF16(@"Mobile Bookmarks"));
  [self waitForSnackBarMessageText:snackBarMessage
      triggeredByTappingItemWithMatcher:grey_allOf(grey_kindOfClassName(
                                                       @"UITableViewCell"),
                                                   grey_descendant(grey_text(
                                                       @"Mobile Bookmarks")),
                                                   nil)];
}

// Tests adding items to the readinglist from the tab grid edit mode.
- (void)testTabGridBulkActionAddToReadingList {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];
  WaitForTabGridFullscreen();

  // Select the first and last items.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(2)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridEditAddToButton()]
      performAction:grey_tap()];

  [self waitForSnackBarMessage:IDS_IOS_READING_LIST_SNACKBAR_MESSAGE
      triggeredByTappingItemWithMatcher:AddToReadingListButton()];
}

// Tests sharing multiple tabs from the tab grid edit mode.
- (void)testTabGridBulkActionShare {
  // TODO(crbug.com/1238501): The pasteboard is "not available at this time"
  // when running on device.

#if !TARGET_OS_SIMULATOR
  EARL_GREY_TEST_SKIPPED(
      @"The pasteboard is inaccessible when running on device.");
#endif

  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];
  WaitForTabGridFullscreen();

  // Select the first and last items.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(2)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridEditShareButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::CopyActivityButton()]
      performAction:grey_tap()];

  NSString* URL1String = base::SysUTF8ToNSString(_URL1.spec());
  NSString* URL3String = base::SysUTF8ToNSString(_URL3.spec());

  // Copying can take a while, wait for it to happen.
  GREYCondition* copyCondition = [GREYCondition
      conditionWithName:@"test text copied condition"
                  block:^BOOL {
                    NSArray<NSString*>* URLStrings =
                        UIPasteboard.generalPasteboard.strings;
                    if (URLStrings.count != 2) {
                      return false;
                    }

                    // Strings may appear in either order.
                    if (([URLStrings[0] isEqualToString:URL1String] &&
                         [URLStrings[1] isEqualToString:URL3String]) ||
                        ([URLStrings[0] isEqualToString:URL3String] &&
                         [URLStrings[1] isEqualToString:URL1String])) {
                      return true;
                    }

                    return false;
                  }];
  // Wait for copy to happen or timeout after 5 seconds.
  GREYAssertTrue([copyCondition waitWithTimeout:5], @"Copying URLs failed");
}

#pragma mark - Tab Grid Search

// Tests entering and exit of the tab grid search mode.
- (void)testEnterExitSearch {
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey showTabSwitcher];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Verify that search mode is active.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSearchModeToolbar()]
      assertWithMatcher:grey_notNil()];

  // Exit search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchCancelButton()]
      performAction:grey_tap()];

  // Verify that normal mode is active.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridNormalModePageControl()]
      assertWithMatcher:grey_notNil()];
}

// Tests that exiting search mode reset the tabs count to the original number.
- (void)testTabGridResetAfterExitingSearch {
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey showTabSwitcher];

  // Enter search mode & search with a query that produce no results.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridSearchBar()]
      performAction:grey_typeText(@"hello")];

  // Verify that search reduced the number of visible tabs.
  [self verifyVisibleTabsCount:0];

  // Exit search mode & verify that tabs grid was reset.
  [[EarlGrey selectElementWithMatcher:TabGridSearchCancelButton()]
      performAction:grey_tap()];
  [self verifyVisibleTabsCount:2];
}

// Tests that the scrim view is always shown when the search bar is empty in the
// search mode.
- (void)testScrimVisibleInSearchModeWhenSearchBarIsEmpty {
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey showTabSwitcher];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Upon entry, the search bar is empty. Verify that scrim is visible.
  [[EarlGrey selectElementWithMatcher:SearchScrim()]
      assertWithMatcher:grey_notNil()];

  // Searching with any query should render scrim invisible.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(@"text")];
  [[EarlGrey selectElementWithMatcher:SearchScrim()]
      assertWithMatcher:grey_nil()];

  // Clearing search bar text should render scrim visible again.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_clearText()];
  [[EarlGrey selectElementWithMatcher:SearchScrim()]
      assertWithMatcher:grey_notNil()];

  // Cancel search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchCancelButton()]
      performAction:grey_tap()];

  // Verify that scrim is not visible anymore.
  [[EarlGrey selectElementWithMatcher:SearchScrim()]
      assertWithMatcher:grey_nil()];
}

// Tests that tapping on the scrim view while in search mode dismisses the scrim
// and exits search mode.
- (void)testTapOnSearchScrimExitsSearchMode {
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey showTabSwitcher];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Tap on scrim.
  [[EarlGrey selectElementWithMatcher:SearchScrim()] performAction:grey_tap()];

  // Verify that search mode is exit, scrim not visible, and transition to
  // normal mode was successful.
  [[EarlGrey selectElementWithMatcher:SearchScrim()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:TabGridNormalModePageControl()]
      assertWithMatcher:grey_notNil()];
  [self verifyVisibleTabsCount:2];
}

// Tests that searching in open tabs in the regular mode will filter the tabs
// correctly.
- (void)testSearchRegularOpenTabs {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGrey showTabSwitcher];

  [self verifyVisibleTabsCount:4];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(@"Page")];

  // Verify that the header of the open tabs section has the correct results
  // count.
  [[EarlGrey selectElementWithMatcher:SearchOpenTabsHeaderWithValue(3)]
      assertWithMatcher:grey_notNil()];

  // Verify that there are 3 results for the query "Page" and they are in the
  // expected order.
  [self verifyVisibleTabsCount:3];
  [[EarlGrey selectElementWithMatcher:TabWithTitleAndIndex(kTitle1, 0)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TabWithTitleAndIndex(kTitle2, 1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TabWithTitleAndIndex(kTitle4, 2)]
      assertWithMatcher:grey_notNil()];

  // Update the search query with one that doesn't match any results.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(@"Foo")];

  // Verify that the header of the open tabs section has 0 as the results count.
  [[EarlGrey selectElementWithMatcher:SearchOpenTabsHeaderWithValue(0)]
      assertWithMatcher:grey_notNil()];

  // Verify that no tabs are visible and previously shown tabs disappeared.
  [self verifyVisibleTabsCount:0];

  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTitle1)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTitle2)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTitle4)]
      assertWithMatcher:grey_nil()];
}

// Tests that open tabs search results header appear only when there is a query
// on the search bar.
- (void)testOpenTabsHeaderVisibleInSearchModeWhenSearchBarIsNotEmpty {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGrey showTabSwitcher];

  // Verify that the header doesn't exist in normal mode.
  [[EarlGrey selectElementWithMatcher:SearchOpenTabsSectionHeader()]
      assertWithMatcher:grey_nil()];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Upon entry, the search bar is empty. Verify that the header doesn't exist.
  [[EarlGrey selectElementWithMatcher:SearchOpenTabsSectionHeader()]
      assertWithMatcher:grey_nil()];

  // Searching with any query should render the header visible.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(@"text\n")];
  [[EarlGrey selectElementWithMatcher:SearchOpenTabsSectionHeader()]
      assertWithMatcher:grey_notNil()];

  // Clearing search bar text should render the header invisible again.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_clearText()];
  [[EarlGrey selectElementWithMatcher:SearchOpenTabsSectionHeader()]
      assertWithMatcher:grey_nil()];

  // Searching a word then canceling the search mode should hide the section
  // header.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(@"page\n")];
  [[EarlGrey selectElementWithMatcher:TabGridSearchCancelButton()]
      performAction:grey_tap()];
  [[self scrollUpViewMatcher:RegularTabGrid()
             toSelectMatcher:SearchOpenTabsSectionHeader()]
      assertWithMatcher:grey_nil()];
}

// Tests that suggested actions section is available whenever there is a query
// in the normal tabs search mode.
- (void)testSuggestedActionsVisibleInSearchModeWhenSearchBarIsNotEmpty {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGrey showTabSwitcher];

  // Verify that the suggested actions section doesn't exist in normal mode.
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSectionHeader()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSection()]
      assertWithMatcher:grey_nil()];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Upon entry, the search bar is empty. Verify that the suggested actions
  // section doesn't exist.
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSectionHeader()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSection()]
      assertWithMatcher:grey_nil()];

  // Searching with a query with no results should show the suggested actions
  // section.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(@"text\n")];
  [[EarlGrey selectElementWithMatcher:SearchOpenTabsHeaderWithValue(0)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSectionHeader()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSection()]
      assertWithMatcher:grey_notNil()];

  // Clearing search bar text should hide the suggested actions section.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_clearText()];
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSectionHeader()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSection()]
      assertWithMatcher:grey_nil()];

  // Searching with a query with results should show the suggested actions
  // section.
  NSString* query = [NSString stringWithFormat:@"%s\n", kTitle2];
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_clearText()];
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(query)];

  // Check that the header is set correctly.
  [[EarlGrey selectElementWithMatcher:SearchOpenTabsHeaderWithValue(1)]
      assertWithMatcher:grey_notNil()];

  [[self scrollDownViewMatcher:RegularTabGrid()
               toSelectMatcher:SearchSuggestedActionsSectionHeader()]
      assertWithMatcher:grey_notNil()];
  [[self scrollDownViewMatcher:RegularTabGrid()
               toSelectMatcher:SearchSuggestedActionsSection()]
      assertWithMatcher:grey_notNil()];

  // Canceling search mode should hide the suggested actions section.
  [[EarlGrey selectElementWithMatcher:TabGridSearchCancelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSectionHeader()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSection()]
      assertWithMatcher:grey_nil()];
}

// Tests that suggested actions section does not appear in search mode for
// incognito page.
- (void)testSuggestedActionsNotAvailableInIncognitoPageSearchMode {
  [self loadTestURLsInNewIncognitoTabs];
  [ChromeEarlGrey showTabSwitcher];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Upon entry, the search bar is empty. Verify that the suggested actions
  // section doesn't exist.
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSectionHeader()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSection()]
      assertWithMatcher:grey_nil()];

  // Searching with a query should not show suggested actions section.
  NSString* query = [NSString stringWithFormat:@"%s\n", kTitle2];
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(query)];
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSectionHeader()]
      assertWithMatcher:grey_nil()];
  [[self scrollDownViewMatcher:chrome_test_util::IncognitoTabGrid()
               toSelectMatcher:SearchSuggestedActionsSection()]
      assertWithMatcher:grey_nil()];
}

// Tests that the search suggested actions section has the right rows in the
// regular grid.
- (void)testSearchSuggestedActionsSectionContentInRegularGrid {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGrey showTabSwitcher];

  // Enter search mode and enter a search query.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];
  // TODO(crbug.com/1306246): Scrolling doesn't work properly in very small
  // devices. Once that is fixed a more broad query can be used for searching
  // (eg. "page").
  NSString* query = [NSString stringWithFormat:@"%s\n", kTitle2];
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(query)];

  // Verify that the suggested actions section exist and has "Search on web",
  // "Search recent tabs", "Search history" rows.
  [[self scrollDownViewMatcher:RegularTabGrid()
               toSelectMatcher:SearchSuggestedActionsSectionHeader()]
      assertWithMatcher:grey_notNil()];

  [[self
      scrollDownViewMatcher:RegularTabGrid()
            toSelectMatcher:grey_allOf(
                                SearchSuggestedActionsSection(),
                                grey_descendant(SearchOnWebSuggestedAction()),
                                grey_descendant(
                                    SearchRecentTabsSuggestedAction()),
                                grey_descendant(SearchHistorySuggestedAction()),
                                grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests that history row in the search suggested actions section displays the
// correct number of matches.
- (void)testSearchSuggestedActionsDisplaysCorrectHistoryMatchesCount {
  [ChromeEarlGrey clearBrowsingHistory];
  [self loadTestURLs];
  [ChromeEarlGrey showTabSwitcher];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Verify that the suggested actions section is not visible.
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSection()]
      assertWithMatcher:grey_nil()];

  // Searching the word "page" matches 2 items from history.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(@"page\n")];
  [[self scrollDownViewMatcher:RegularTabGrid()
               toSelectMatcher:
                   SearchSuggestedActionsSectionWithHistoryMatchesCount(2)]
      assertWithMatcher:grey_notNil()];

  // Adding to the existing query " two" will search for "page two" and should
  // only match 1 item from the history.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(@" two\n")];
  [[self scrollDownViewMatcher:RegularTabGrid()
               toSelectMatcher:
                   SearchSuggestedActionsSectionWithHistoryMatchesCount(1)]
      assertWithMatcher:grey_notNil()];

  // Cancel search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchCancelButton()]
      performAction:grey_tap()];
}

// Tests that selecting an open tab search result in the regular mode will
// correctly open the expected tab.
- (void)testSearchRegularOpenTabsSelectResult {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGrey showTabSwitcher];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  NSString* title2 = base::SysUTF8ToNSString(kTitle2);
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(title2)];

  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTitle2)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];
  const GURL currentURL = [ChromeEarlGrey webStateVisibleURL];
  GREYAssertEqual(_URL2, currentURL, @"Page navigated unexpectedly to %s",
                  currentURL.spec().c_str());
}

// Tests that selecting an open tab search result in incognito mode will
// correctly open the expected tab.
- (void)testSearchIncognitoOpenTabsSelectResult {
  [self loadTestURLsInNewIncognitoTabs];
  [ChromeEarlGrey showTabSwitcher];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  NSString* title2 = base::SysUTF8ToNSString(kTitle2);
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(title2)];

  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTitle2)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];
  const GURL currentURL = [ChromeEarlGrey webStateVisibleURL];
  GREYAssertEqual(_URL2, currentURL, @"Page navigated unexpectedly to %s",
                  currentURL.spec().c_str());
}

- (void)testSearchOpenTabsContextMenuShare {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGrey showTabSwitcher];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  NSString* title2 = base::SysUTF8ToNSString(kTitle2);
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(title2)];

  [self longPressTabWithTitle:title2];

  [ChromeEarlGrey verifyShareActionWithURL:_URL1 pageTitle:title2];
}

- (void)testSearchOpenTabsContextMenuAddToReadingList {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGrey showTabSwitcher];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  NSString* title2 = base::SysUTF8ToNSString(kTitle2);
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(title2)];

  [self longPressTabWithTitle:title2];

  [self waitForSnackBarMessage:IDS_IOS_READING_LIST_SNACKBAR_MESSAGE
      triggeredByTappingItemWithMatcher:AddToReadingListButton()];
}

- (void)testSearchOpenTabsContextMenuAddToBookmarks {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGrey showTabSwitcher];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  NSString* title2 = base::SysUTF8ToNSString(kTitle2);
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(title2)];

  [self longPressTabWithTitle:title2];

  [self waitForSnackBarMessage:IDS_IOS_BOOKMARK_PAGE_SAVED
      triggeredByTappingItemWithMatcher:AddToBookmarksButton()];

  [self longPressTabWithTitle:title2];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_TOOLS_MENU_EDIT_BOOKMARK)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
                     IDS_IOS_BOOKMARK_EDIT_SCREEN_TITLE)]
      assertWithMatcher:grey_notNil()];
}

- (void)testSearchOpenTabsContextMenuCloseTab {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGrey showTabSwitcher];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  NSString* title2 = base::SysUTF8ToNSString(kTitle2);
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_typeText(title2)];

  [self longPressTabWithTitle:title2];

  // Close Tab.
  [[EarlGrey selectElementWithMatcher:CloseTabMenuButton()]
      performAction:grey_tap()];

  // Make sure that the tab is no longer present.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTitle2)]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Helper Methods

- (void)loadTestURLs {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];
}

- (void)loadTestURLsInNewTabs {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL4];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse4];
}

- (void)loadTestURLsInNewIncognitoTabs {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL4];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse4];
}

// Loads a URL in a new tab and deletes it to populate Recent Tabs. Then,
// navigates to the Recent tabs via tab grid.
- (void)prepareRecentTabWithURL:(const GURL&)URL
                       response:(const char*)response {
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:response];

  // Close the tab, making it appear in Recent Tabs.
  [ChromeEarlGrey closeCurrentTab];

  // Switch over to Recent Tabs.
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
}

// Long press on the recent tab entry or the tab item in the tab grid with
// |title|.
- (void)longPressTabWithTitle:(NSString*)title {
  // The test page may be there multiple times.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(title),
                                          grey_sufficientlyVisible(), nil)]
      atIndex:0] performAction:grey_longPress()];
}

// Checks if the content of the given tab in the given window matches given
// text. This method exits the tab grid and re-enters it afterward.
- (void)fromGridCheckTabAtIndex:(int)tabIndex
                 inWindowNumber:(int)windowNumber
                   containsText:(const char*)text {
  [EarlGrey
      setRootMatcherForSubsequentInteractions:WindowWithNumber(windowNumber)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(
                                          tabIndex)] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:text
                             inWindowWithNumber:windowNumber];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
}

- (void)waitForSnackBarMessage:(int)messageIdentifier
    triggeredByTappingItemWithMatcher:(id<GREYMatcher>)matcher {
  NSString* snackBarLabel = l10n_util::GetNSStringWithFixup(messageIdentifier);
  // Start custom monitor, because there's a chance the snackbar is
  // already gone by the time we wait for it (and it was like that sometimes).
  [ChromeEarlGrey watchForButtonsWithLabels:@[ snackBarLabel ]
                                    timeout:kSnackbarAppearanceTimeout];

  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];

  // Wait for the snackbar to appear.
  id<GREYMatcher> snackbar_matcher =
      chrome_test_util::ButtonWithAccessibilityLabelId(messageIdentifier);
  ConditionBlock wait_for_appearance = ^{
    return [ChromeEarlGrey watcherDetectedButtonWithLabel:snackBarLabel];
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kSnackbarAppearanceTimeout, wait_for_appearance),
             @"Snackbar did not appear.");

  // Wait for the snackbar to disappear.
  ConditionBlock wait_for_disappearance = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:snackbar_matcher]
        assertWithMatcher:grey_nil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kSnackbarDisappearanceTimeout, wait_for_disappearance),
             @"Snackbar did not disappear.");
}

- (void)waitForSnackBarMessageText:(NSString*)snackBarLabel
    triggeredByTappingItemWithMatcher:(id<GREYMatcher>)matcher {
  // Start custom monitor, because there's a chance the snackbar is
  // already gone by the time we wait for it (and it was like that sometimes).
  [ChromeEarlGrey watchForButtonsWithLabels:@[ snackBarLabel ]
                                    timeout:kSnackbarAppearanceTimeout];

  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];

  // Wait for the snackbar to appear.
  id<GREYMatcher> snackbar_matcher =
      chrome_test_util::ButtonWithAccessibilityLabel(snackBarLabel);
  ConditionBlock wait_for_appearance = ^{
    return [ChromeEarlGrey watcherDetectedButtonWithLabel:snackBarLabel];
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kSnackbarAppearanceTimeout, wait_for_appearance),
             @"Snackbar did not appear.");

  // Wait for the snackbar to disappear.
  ConditionBlock wait_for_disappearance = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:snackbar_matcher]
        assertWithMatcher:grey_nil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kSnackbarDisappearanceTimeout, wait_for_disappearance),
             @"Snackbar did not disappear.");
}

// Verifies that the tab grid has exactly |expectedCount| tabs.
- (void)verifyVisibleTabsCount:(NSUInteger)expectedCount {
  // Verify that the cell # |expectedCount| exist.
  if (expectedCount == 0) {
    [[EarlGrey selectElementWithMatcher:TabGridCell()]
        assertWithMatcher:grey_nil()];
  } else {
    [[[EarlGrey selectElementWithMatcher:TabGridCell()]
        atIndex:expectedCount - 1] assertWithMatcher:grey_notNil()];
  }
  // Then verify that there is no more cells after that.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(TabGridCell(),
                                          TabGridCellAtIndex(expectedCount),
                                          nil)] assertWithMatcher:grey_nil()];
}

// Returns an interaction that scrolls down on the view matched by |viewMatcher|
// to search for the given |matcher|.
- (id<GREYInteraction>)scrollDownViewMatcher:(id<GREYMatcher>)viewMatcher
                             toSelectMatcher:(id<GREYMatcher>)matcher {
  return [[EarlGrey selectElementWithMatcher:matcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:viewMatcher];
}

// Returns an interaction that scrolls up on the view matched by |viewMatcher|
// to search for the given |matcher|.
- (id<GREYInteraction>)scrollUpViewMatcher:(id<GREYMatcher>)viewMatcher
                           toSelectMatcher:(id<GREYMatcher>)matcher {
  return [[EarlGrey selectElementWithMatcher:matcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionUp, 50)
      onElementWithMatcher:viewMatcher];
}

@end
