// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/containers/contains.h"
#import "base/format_macros.h"
#import "base/i18n/message_formatter.h"
#import "base/ios/ios_util.h"
#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/commerce/core/proto/price_tracking.pb.h"
#import "components/unified_consent/pref_names.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/history/ui_bundled/history_ui_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_test_app_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_app_interface.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_app_interface.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/settings/settings_app_interface.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/test/tabs_egtest_util.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/data_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

using base::test::ios::kWaitForUIElementTimeout;
using chrome_test_util::AddToBookmarksButton;
using chrome_test_util::AddToReadingListButton;
using chrome_test_util::CloseTabMenuButton;
using chrome_test_util::IncognitoTabGrid;
using chrome_test_util::LongPressCellAndDragToEdge;
using chrome_test_util::LongPressCellAndDragToOffsetOf;
using chrome_test_util::RegularTabGrid;
using chrome_test_util::TabGridCellAtIndex;
using chrome_test_util::TabGridEditMenuCloseAllButton;
using chrome_test_util::TabGridInactiveTabsButton;
using chrome_test_util::TabGridIncognitoTabsPanelButton;
using chrome_test_util::TabGridNormalModePageControl;
using chrome_test_util::TabGridOpenTabsPanelButton;
using chrome_test_util::TabGridOtherDevicesPanelButton;
using chrome_test_util::TabGridSearchBar;
using chrome_test_util::TabGridSearchCancelButton;
using chrome_test_util::TabGridSearchModeToolbar;
using chrome_test_util::TabGridSearchTabsButton;
using chrome_test_util::TabGridSelectTabsMenuButton;
using chrome_test_util::TabGridTabGroupsPanelButton;
using chrome_test_util::TabGridThirdPanelButton;
using chrome_test_util::TapAtOffsetOf;
using chrome_test_util::WindowWithNumber;

namespace {
const char kSearchEngineURL[] = "http://searchengine/?q={searchTerms}";
const char kSearchEngineHost[] = "searchengine";

char kURL1[] = "http://firstURL";
char kURL2[] = "http://secondURL";
char kURL3[] = "http://thirdURL";
char kURL4[] = "http://fourthURL";
NSString* const kTitle1 = @"Page one";
NSString* const kTitle2 = @"Page two";
NSString* const kTitle4 = @"Page four";
char kResponse1[] = "Test Page 1 content";
char kResponse2[] = "Test Page 2 content";
char kResponse3[] = "Test Page 3 content";
char kResponse4[] = "Test Page 4 content";
NSString* const kTypeURL =
    @"type.googleapis.com/optimization_guide.proto.PriceTrackingData";
const int64_t kCurrentPriceMicros = 5'000'000;
const int64_t kPreviousPriceMicros = 10'000'000;
const int64_t kOfferId = 50;
const char kCurrencyCode[] = "USD";
NSString* const kExpectedCurrentPrice = @"$5.00";
NSString* const kExpectedPreviousPrice = @"$10";

id<GREYMatcher> RecentlyClosedTabWithTitle(NSString* title) {
  return grey_allOf(grey_ancestor(grey_accessibilityID(
                        kRecentTabsTableViewControllerAccessibilityIdentifier)),
                    chrome_test_util::StaticTextWithAccessibilityLabel(title),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> RecentlyClosedTabsSectionHeader() {
  return grey_allOf(chrome_test_util::HeaderWithAccessibilityLabelId(
                        IDS_IOS_RECENT_TABS_RECENTLY_CLOSED),
                    grey_sufficientlyVisible(), nil);
}

// Identifer for cell at given `index` in the tab grid.
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

// Returns the matcher for the Recent Tabs table.
id<GREYMatcher> RecentTabsTable() {
  return grey_allOf(grey_accessibilityID(
                        kRecentTabsTableViewControllerAccessibilityIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the scrim view on the tab search.
id<GREYMatcher> VisibleSearchScrim() {
  return grey_allOf(grey_accessibilityID(kTabGridScrimIdentifier),
                    grey_minimumVisiblePercent(0.5), nil);
}

// Returns a matcher for the search bar text field containing `searchText`.
id<GREYMatcher> SearchBarWithSearchText(NSString* searchText) {
  return grey_accessibilityID([kTabGridSearchTextFieldIdentifierPrefix
      stringByAppendingString:searchText]);
}

// Returns a matcher for the search results header with title set with
// `title_id`.
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
// `count` set in the value label .
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

// Returns a matcher for the "Search open tabs" suggested action.
id<GREYMatcher> SearchOpenTabsSuggestedAction() {
  return grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabelId(
                        IDS_IOS_TABS_SEARCH_SUGGESTED_ACTION_SEARCH_OPEN_TABS),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the "Search history" suggested action.
id<GREYMatcher> SearchHistorySuggestedAction() {
  return grey_allOf(
      grey_accessibilityID(kTableViewTabsSearchSuggestedHistoryItemId),
      grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the "Search history (`matches_count` Found)" suggested
// action on the regular tab grid.
id<GREYMatcher> SearchHistorySuggestedActionWithMatches(size_t matches_count) {
  NSString* count_str = [NSString stringWithFormat:@"%" PRIuS, matches_count];
  NSString* history_label = l10n_util::GetNSStringF(
      IDS_IOS_TABS_SEARCH_SUGGESTED_ACTION_SEARCH_HISTORY,
      base::SysNSStringToUTF16(count_str));
  return grey_allOf(grey_accessibilityLabel(history_label),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> SelectedStateTitleSelection(int selection_count) {
  NSString* title =
      selection_count == 0
          ? l10n_util::GetNSString(IDS_IOS_TAB_GRID_SELECT_TABS_TITLE)
          : l10n_util::GetPluralNSStringF(IDS_IOS_TAB_GRID_SELECTED_TABS_TITLE,
                                          selection_count);
  return grey_allOf(grey_accessibilityLabel(title),
                    grey_kindOfClassName(@"_UIButtonBarButton"), nil);
}

// Returns a matcher for the "Search history (`matches_count` Found)" suggested
// action on the recent tabs page.
id<GREYMatcher> RecentTabsSearchHistorySuggestedActionWithMatches(
    size_t matches_count) {
  return grey_allOf(
      RecentTabsTable(),
      grey_descendant(SearchHistorySuggestedActionWithMatches(matches_count)),
      nil);
}

// Returns a matcher for the search suggested actions section.
id<GREYMatcher> SearchSuggestedActionsSection() {
  return grey_allOf(grey_accessibilityID(kSuggestedActionsGridCellIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the search suggested actions section with the history
// item matches count set to `matches_count`.
id<GREYMatcher> SearchSuggestedActionsSectionWithHistoryMatchesCount(
    size_t matches_count) {
  return grey_allOf(
      SearchSuggestedActionsSection(),
      grey_descendant(SearchHistorySuggestedActionWithMatches(matches_count)),
      grey_sufficientlyVisible(), nil);
}
// Matcher for the select tabs button in the context menu.
id<GREYMatcher> SelectTabsContextMenuItem() {
  return chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_SELECTTABS);
}

// Type `text` into the TabGridSearchBar and press enter.
void PerformTabGridSearch(NSString* text) {
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(text)];
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];
}

#pragma mark - TestResponseProvider

// A ResponseProvider that provides html responses of the requested URL for
// requests to `kSearchEngineHost`.
class EchoURLDefaultSearchEngineResponseProvider
    : public web::DataResponseProvider {
 public:
  bool CanHandleRequest(const Request& request) override;
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override;
};

bool EchoURLDefaultSearchEngineResponseProvider::CanHandleRequest(
    const Request& request) {
  return base::Contains(request.url.spec(), kSearchEngineHost);
}

void EchoURLDefaultSearchEngineResponseProvider::GetResponseHeadersAndBody(
    const Request& request,
    scoped_refptr<net::HttpResponseHeaders>* headers,
    std::string* response_body) {
  const GURL& url = request.url;
  *headers = web::ResponseProvider::GetDefaultResponseHeaders();
  std::string url_string = base::ToLowerASCII(url.spec());
  *response_body =
      base::StringPrintf("<html><body>%s</body></html>", url_string.c_str());
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

- (void)setUp {
  [super setUp];

  _URL1 = web::test::HttpServer::MakeUrl(kURL1);
  _URL2 = web::test::HttpServer::MakeUrl(kURL2);
  _URL3 = web::test::HttpServer::MakeUrl(kURL3);
  _URL4 = web::test::HttpServer::MakeUrl(kURL4);

  std::map<GURL, std::string> responses;
  const char kPageFormat[] = "<head><title>%s</title></head><body>%s</body>";
  responses[_URL1] = base::StringPrintf(
      kPageFormat, base::SysNSStringToUTF8(kTitle1).c_str(), kResponse1);
  responses[_URL2] = base::StringPrintf(
      kPageFormat, base::SysNSStringToUTF8(kTitle2).c_str(), kResponse2);
  // Page 3 does not have <title> tag, so URL will be its title.
  responses[_URL3] = kResponse3;
  responses[_URL4] = base::StringPrintf(
      kPageFormat, base::SysNSStringToUTF8(kTitle4).c_str(), kResponse4);
  web::test::SetUpSimpleHttpServer(responses);
}

- (void)tearDown {
  // Ensure that the default search engine is reset.
  if ([self isRunningTest:@selector
            (testSearchOnWebSuggestedActionInRegularTabsSearch)] ||
      [self isRunningTest:@selector
            (testSearchOnWebSuggestedActionInRecentTabsSearch)]) {
    [SettingsAppInterface resetSearchEngine];
  }

  // Ensure that pref set in testTabGridItemContextMenuAddToBookmarkGreyed is
  // reset even if the test failed.
  if ([self isRunningTest:@selector
            (testTabGridItemContextMenuAddToBookmarkGreyed)]) {
    [ChromeEarlGrey setBoolValue:YES
                     forUserPref:bookmarks::prefs::kEditBookmarksEnabled];
  }

  // Shutdown network process after tests run to avoid hanging from
  // clearing browsing data.
  // See https://crbug.com/1419875.
  [ChromeEarlGrey killWebKitNetworkProcess];

  // Wait for the end of sign-out before starting following tests.
  // See https://crbug.com/1448618.
  // Should be removed after TODO(crbug.com/40065405).
  if ([self isRunningTest:@selector
            (DISABLED_testSyncSpinnerDismissedInRecentlyClosedTabs)]) {
    [ChromeEarlGrey signOutAndClearIdentities];
  }

  [super tearDown];
}

// Tests entering and leaving the tab grid.
- (void)testEnteringAndLeavingTabGrid {
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests that tapping on the first cell shows that tab.
- (void)testTappingOnFirstCell {
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that closing the cell shows no tabs, and displays the empty state.
- (void)testClosingFirstCell {
  [ChromeEarlGreyUI openTabGrid];
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
// the empty state, and ensures that it doesn't affect the other tab grid. Then
// tests tapping Undo shows Close All button again. Validates this case when Tab
// Grid Bulk Actions feature is enabled.
- (void)testCloseAllAndUndoCloseAll {
  // Also add a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  // Open tab grid and go to regular tab page.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridNormalModePageControl()]
      performAction:grey_tap()];

  // Close all tabs.
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridEditMenuCloseAllButton()]
      performAction:grey_tap()];

  // Ensure normal tabs were closed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_nil()];

  // Ensure the incognito tab isn't closed.
  GREYAssertEqual(1, [ChromeEarlGrey incognitoTabCount],
                  @"Expected that the \"Close All Tabs\" button should not "
                  @"close tabs in other pages.");

  // Ensure undo button is visible and edit button is not visible.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridUndoCloseAllButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridRegularTabsEmptyStateView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Undo button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridUndoCloseAllButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tapping Close All also closes inactive tabs. Ensures it is
// correctly recovered when pressing undo and there is no selection mode when
// there are inactive tabs but no regular tabs.
- (void)testCloseAllAndUndoCloseAllWithInactiveTabs {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }
  [self loadTestURLsInNewTabs];
  [self relaunchAppWithInactiveTabsEnabled];

  [ChromeEarlGreyUI openTabGrid];
  GREYAssertEqual(1, [ChromeEarlGrey mainTabCount],
                  @"Expected only one tab (NTP), all other tabs should have "
                  @"been in inactive tab grid.");
  GREYAssertEqual(4, [ChromeEarlGrey inactiveTabCount],
                  @"Expected 4 inactive tabs.");

  // Verify that the Inactive Tabs button is showing.
  [[EarlGrey selectElementWithMatcher:TabGridInactiveTabsButton()]
      assertWithMatcher:grey_notNil()];

  // Ensure the edit button is visible.
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close all tabs.
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridEditMenuCloseAllButton()]
      performAction:grey_tap()];

  // Ensure regular and inactive tabs were closed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridRegularTabsEmptyStateView()]
      assertWithMatcher:grey_sufficientlyVisible()];
  GREYAssertEqual(0, [ChromeEarlGrey mainTabCount],
                  @"Expected all regular tab to be closed.");
  GREYAssertEqual(0, [ChromeEarlGrey inactiveTabCount],
                  @"Expected all inactive tab to be closed.");

  // Verify that the Inactive Tabs button is not showing.
  [[EarlGrey selectElementWithMatcher:TabGridInactiveTabsButton()]
      assertWithMatcher:grey_nil()];

  // Tap Undo button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridUndoCloseAllButton()]
      performAction:grey_tap()];
  GREYAssertEqual(1, [ChromeEarlGrey mainTabCount],
                  @"Expected only one tab (NTP), all other tabs should have "
                  @"been in inactive tab grid.");
  GREYAssertEqual(4, [ChromeEarlGrey inactiveTabCount],
                  @"Expected 4 inactive tabs.");
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the Inactive Tabs button is showing again.
  [[EarlGrey selectElementWithMatcher:TabGridInactiveTabsButton()]
      assertWithMatcher:grey_notNil()];

  // Closing the only tab in the regular tab grid and verify there is an empty
  // grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridRegularTabsEmptyStateView()]
      assertWithMatcher:grey_sufficientlyVisible()];
  GREYAssertEqual(0, [ChromeEarlGrey mainTabCount],
                  @"Expected no tab in regular tab grid.");
  GREYAssertEqual(4, [ChromeEarlGrey inactiveTabCount],
                  @"Expected 4 inactive tabs.");

  // Ensure there is no selection mode available when there is no tab in regular
  // tab grid.
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectTabsMenuButton()]
      assertWithMatcher:grey_nil()];

  // Close all.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridEditMenuCloseAllButton()]
      performAction:grey_tap()];
  GREYAssertEqual(0, [ChromeEarlGrey mainTabCount],
                  @"Expected all regular tab to be closed.");
  GREYAssertEqual(0, [ChromeEarlGrey inactiveTabCount],
                  @"Expected all inactive tab to be closed.");

  // Tap on Undo button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridUndoCloseAllButton()]
      performAction:grey_tap()];
  GREYAssertEqual(0, [ChromeEarlGrey mainTabCount],
                  @"Expected no tab in regular tab grid.");
  GREYAssertEqual(4, [ChromeEarlGrey inactiveTabCount],
                  @"Expected 4 inactive tabs.");
}

- (void)testPriceDrops {
  commerce::PriceTrackingData price_tracking_data;
  price_tracking_data.mutable_product_update()->set_offer_id(kOfferId);
  price_tracking_data.mutable_product_update()
      ->mutable_old_price()
      ->set_currency_code(kCurrencyCode);
  price_tracking_data.mutable_product_update()
      ->mutable_new_price()
      ->set_currency_code(kCurrencyCode);
  price_tracking_data.mutable_product_update()
      ->mutable_new_price()
      ->set_amount_micros(kCurrentPriceMicros);
  price_tracking_data.mutable_product_update()
      ->mutable_old_price()
      ->set_amount_micros(kPreviousPriceMicros);

  std::string serialized_price_tracking_data;
  price_tracking_data.SerializeToString(&serialized_price_tracking_data);
  [OptimizationGuideTestAppInterface
      addHintForTesting:base::SysUTF8ToNSString(_URL1.spec())
                   type:optimization_guide::proto::OptimizationType::
                            PRICE_TRACKING
         serialized_any:[[NSData alloc]
                            initWithBytes:serialized_price_tracking_data.c_str()
                                   length:serialized_price_tracking_data.size()]
               type_url:kTypeURL];
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:unified_consent::prefs::
                                   kUrlKeyedAnonymizedDataCollectionEnabled];
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:prefs::kTrackPricesOnTabsEnabled];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];
  [ChromeEarlGreyUI openTabGrid];
  id<GREYMatcher> new_price = grey_text(kExpectedCurrentPrice);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:new_price];
  id<GREYMatcher> prev_price = grey_text(kExpectedPreviousPrice);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:prev_price];
}

// Tests that tapping Close All from the incognito grid shows no tabs and does
// not shows Undo button. Also ensure that it close the expected tabs to avoid
// crbug.com/1475005.
- (void)testCloseAllAndUndoCloseAllForIncognitoGrid {
  // Opens 3 incognito tabs and 1 regular.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey openNewTab];

  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey waitForIncognitoTabCount:3];

  // Open the regular grid.
  [ChromeEarlGreyUI openTabGrid];

  // Scroll the tab grid to switch from regular grid to incognito grid.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kTabGridScrollViewIdentifier),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeLeft)];

  // Close all incognito tabs
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridEditMenuCloseAllButton()]
      performAction:grey_tap()];

  // Ensure only incognito tabs were closed
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  // Ensure undo button is not visible and edit button is visible
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridUndoCloseAllButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the Undo button is no longer available after tapping Close All,
// then creating a new tab, then coming back to the tab grid.
// Validates this case when Tab Grid Bulk Actions feature is enabled.
- (void)testUndoCloseAllNotAvailableAfterNewTabCreation {
  [ChromeEarlGreyUI openTabGrid];

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

  [ChromeEarlGreyUI openTabGrid];
  // Undo is no longer available.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridUndoCloseAllButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests simulating a swipe with Voice Over from the third panel, making sure
// that the new tab button is working as expected.
- (void)testSwipeUsingVoiceOver {
  [ChromeEarlGreyUI openTabGrid];

  // Switch over to the third panel.
  [[EarlGrey selectElementWithMatcher:TabGridThirdPanelButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridNewTabButton()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTabGridScrollViewIdentifier)]
      performAction:chrome_test_util::AccessibilitySwipeRight()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridNewTabButton()]
      performAction:grey_tap()];
}

// Tests that Clear Browsing Data can be successfully done from tab grid.
- (void)FLAKY_testClearBrowsingData {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the test flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when Tab "
                           @"Group Sync is enabled.");
  }

  // Load history.
  [self loadTestURLs];

  [ChromeEarlGreyUI openTabGrid];

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

// Tests that the user interface style is respected after a drag and drop.
// TODO(crbug.com/368385383): Test flaky on iOS.
- (void)DISABLED_testTraitCollection {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGreyUI openTabGrid];

  GREYAssert(chrome_test_util::LongPressCellAndDragToOffsetOf(
                 IdentifierForCellAtIndex(0), 0, IdentifierForCellAtIndex(1), 0,
                 CGVectorMake(0.5, 0.5)),
             @"Failed to DND cell on window");

  GREYMatchesBlock match = ^BOOL(UIView* element) {
    return element.traitCollection.userInterfaceStyle ==
           UIUserInterfaceStyleLight;
  };

  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"Wrong style"];
  };

  id<GREYMatcher> matcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:match
                                           descriptionBlock:describe];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          IdentifierForCellAtIndex(0))]
      assertWithMatcher:matcher];
}

// Tests that the incognito buttons are correctly displayed (regression for
// crbug.com/359698935).
- (void)testIncognitoButtons {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

#pragma mark - Recent Tabs

// Tests reopening a closed tab from an incognito tab.
- (void)testOpenCloseTabFromIncognito {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is only reachable via the tools menu in Regular mode. So the
  // test flow is not supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when Tab "
                           @"Group Sync is enabled.");
  }

  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  // Close the tab, making it appear in Recent Tabs.
  [ChromeEarlGrey closeAllNormalTabs];

  [ChromeEarlGrey openNewIncognitoTab];

  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForMainTabCount:0];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(kTitle1),
                                          grey_sufficientlyVisible(), nil)]
      atIndex:0] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(_URL1.GetContent())];

  [ChromeEarlGrey waitForMainTabCount:1];
  [ChromeEarlGrey waitForIncognitoTabCount:1];
}

// Tests that the Done button is disabled if there is no tab in the last active
// page. This also ensures that the last active page is the correct one
// (incognito if the last opened tab was incognito and regular if the last
// active page was regular), so the Done button opens a tab in the correct page.
// (Do not open a regular tab if the active page is an incognito one).
- (void)testRecentTabDoneButtonAndLastActivePage {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the test flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when Tab "
                           @"Group Sync is enabled.");
  }

  // Load 1 regular tab.
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];
  [ChromeEarlGrey waitForMainTabCount:1];
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  // Open 1 incognito tab.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];
  [ChromeEarlGrey waitForMainTabCount:1];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  // Go to regular and open the previously created tab.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  [self verifyVisibleTabsCount:1];
  [[EarlGrey selectElementWithMatcher:TabWithTitleAndIndex(kTitle1, 0)]
      performAction:grey_tap()];

  // Go to the Recent Tabs panel and tap the Done button.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Ensure that we opened a regular tab as the last open tab was a regular one.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:RegularTabGrid()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Go to incognito and open the previously created tab.
  [[EarlGrey selectElementWithMatcher:TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];
  [self verifyVisibleTabsCount:1];
  [[EarlGrey selectElementWithMatcher:TabWithTitleAndIndex(kTitle2, 0)]
      performAction:grey_tap()];

  // Go to the Recent Tabs panel and tap the Done button.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Ensure that we opened an incognito tab as the last open tab was an
  // incognito one.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:IncognitoTabGrid()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close the only incognito tab.
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridEditMenuCloseAllButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForMainTabCount:1];
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  // Go to the Recent Tabs panel.
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
  // Ensure the Done button is disabled.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
}

- (void)testTabGroupsDoneButtonAndRegularTabs {
  // When Tab Group Sync is disabled, Tab Groups is not present in Tab Grid.
  if (![ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Tab Groups panel is not available in Tab Grid when"
                           @"Tab Group Sync is disabled.");
  }

  // Load 1 regular tab.
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];
  [ChromeEarlGrey waitForMainTabCount:1];
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  // Go to regular and open the previously created tab.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  [self verifyVisibleTabsCount:1];
  [[EarlGrey selectElementWithMatcher:TabWithTitleAndIndex(kTitle1, 0)]
      performAction:grey_tap()];

  // Go to the Tab Groups panel and tap the Done button.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Ensure that we opened a regular tab.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:RegularTabGrid()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close the only regular tab.
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridEditMenuCloseAllButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForMainTabCount:0];
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  // Go to the Tab Groups panel.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];
  // Ensure the Done button is disabled.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
}

#pragma mark - Recent Tabs Context Menu

// Tests the Copy Link action on a Recent Tabs' context menu.
- (void)testRecentTabsContextMenuCopyLink {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the test flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when Tab "
                           @"Group Sync is enabled.");
  }

  [self prepareRecentTabWithURL:_URL1 response:kResponse1];
  [self longPressTabWithTitle:kTitle1];

  [ChromeEarlGrey
      verifyCopyLinkActionWithText:[NSString
                                       stringWithUTF8String:_URL1.spec()
                                                                .c_str()]];
}

// Tests the Open in New Window action on a Recent Tabs' context menu.
- (void)testRecentTabsContextMenuOpenInNewWindow {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the test flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when Tab "
                           @"Group Sync is enabled.");
  }
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  [self prepareRecentTabWithURL:_URL1 response:kResponse1];
  [self longPressTabWithTitle:kTitle1];

  [ChromeEarlGrey verifyOpenInNewWindowActionWithContent:kResponse1];
}

// Tests the Share action on a Recent Tabs' context menu.
- (void)testRecentTabsContextMenuShare {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the test flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when Tab "
                           @"Group Sync is enabled.");
  }
  [self prepareRecentTabWithURL:_URL1 response:kResponse1];
  [self longPressTabWithTitle:kTitle1];

  [ChromeEarlGrey verifyShareActionWithURL:_URL1 pageTitle:kTitle1];
}

// Tests the Serach action on a Recent Tabs.
- (void)testRecentTabsSearch {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the test flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when Tab "
                           @"Group Sync is enabled.");
  }
  [self prepareRecentTabWithURL:_URL1 response:kResponse1];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Verify that search mode is active.
  [[EarlGrey selectElementWithMatcher:TabGridSearchModeToolbar()]
      assertWithMatcher:grey_notNil()];

  // Exit search mode.
  [[EarlGrey selectElementWithMatcher:VisibleSearchScrim()]
      performAction:grey_tap()];

  // Verify that normal mode is active.
  [[EarlGrey selectElementWithMatcher:TabGridNormalModePageControl()]
      assertWithMatcher:grey_notNil()];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Verify that search mode is active.
  [[EarlGrey selectElementWithMatcher:TabGridSearchModeToolbar()]
      assertWithMatcher:grey_notNil()];
}

#pragma mark - Tab Grid Item Context Menu

// Tests the Share action on a tab grid item's context menu.
- (void)testTabGridItemContextMenuShare {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGreyUI openTabGrid];

  [self longPressTabWithTitle:kTitle1];

  [ChromeEarlGrey verifyShareActionWithURL:_URL1 pageTitle:kTitle1];
}

// Tests the Add to Reading list action on a tab grid item's context menu.
- (void)testTabGridItemContextMenuAddToReadingList {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGreyUI openTabGrid];

  [self longPressTabWithTitle:kTitle1];

  [self waitForSnackBarMessage:IDS_IOS_READING_LIST_SNACKBAR_MESSAGE
      triggeredByTappingItemWithMatcher:AddToReadingListButton()];
}

// Tests the Add to Bookmarks action on a tab grid item's context menu.
- (void)testTabGridItemContextMenuAddToBookmarks {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [ChromeEarlGreyUI openTabGrid];

  [self longPressTabWithTitle:kTitle1];

  NSString* snackbarMessage = base::SysUTF16ToNSString(
      l10n_util::GetPluralStringFUTF16(IDS_IOS_BOOKMARKS_BULK_SAVED, 1));
  [self waitForSnackBarMessageText:snackbarMessage
      triggeredByTappingItemWithMatcher:AddToBookmarksButton()];

  // The snackbar disappearance might have been faster than the context menu
  // disappearance. Wait for it to disappear.
  [self waitForContextMenuToDisappear];

  [self longPressTabWithTitle:kTitle1];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_TOOLS_MENU_EDIT_BOOKMARK)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
                     IDS_IOS_BOOKMARK_EDIT_SCREEN_TITLE)]
      assertWithMatcher:grey_notNil()];

  [BookmarkEarlGrey
      verifyExistenceOfBookmarkWithURL:base::SysUTF8ToNSString(_URL1.spec())
                                  name:kTitle1
                             inStorage:BookmarkStorageType::kLocalOrSyncable];
}

// Tests that Add to Bookmarks action is greyed out when editBookmarksEnabled
// pref is set to false.
- (void)testTabGridItemContextMenuAddToBookmarkGreyed {
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:bookmarks::prefs::kEditBookmarksEnabled];

  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGreyUI openTabGrid];

  [self longPressTabWithTitle:kTitle1];
  [[EarlGrey selectElementWithMatcher:AddToBookmarksButton()]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:bookmarks::prefs::kEditBookmarksEnabled];
}

// Tests the Share action on a tab grid item's context menu.
- (void)testTabGridItemContextCloseTab {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGreyUI openTabGrid];

  [self longPressTabWithTitle:kTitle1];

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

- (void)testTabGridItemContextSelectTabs {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGreyUI openTabGrid];

  [self longPressTabWithTitle:kTitle1];

  [[EarlGrey selectElementWithMatcher:SelectTabsContextMenuItem()]
      performAction:grey_tap()];

  // Wait for the select all button to appear to confirm that edit mode was
  // entered.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::TabGridEditSelectAllButton()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    if (error == nil) {
      // Bypass egtest bug on iOS 15 in which the grid cell element briefly
      // unselectable.
      [[EarlGrey
          selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
          assertWithMatcher:grey_sufficientlyVisible()
                      error:&error];
    }
    return (error == nil);
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kWaitForUIElementTimeout, condition),
             @"Wait for select all button to appear in tab grid mode.");

  // Confirm that the tab is selectable.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  NSString* tabSelected = base::SysUTF16ToNSString(
      l10n_util::GetPluralStringFUTF16(IDS_IOS_TAB_GRID_SELECTED_TABS_TITLE,
                                       /*number=*/1));
  [[EarlGrey selectElementWithMatcher:grey_text(tabSelected)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - Drag and drop in Multiwindow

// Tests that dragging a tab grid item to the edge opens a new window and that
// the tab is properly transferred, including navigation stack.
- (void)testDragAndDropAtEdgeToCreateNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");

  // TODO(crbug.com/40752508): Test is failing on iPad devices and simulator.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"This test is failing.");
  }

  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];
  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:0];

  [ChromeEarlGreyUI openTabGrid];

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

  [ChromeEarlGreyUI openTabGrid];

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
// TODO(crbug.com/40868899): Flaky on iPad devices and simulators.
- (void)FLAKY_testDragAndDropBetweenWindows {
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

  // Open tab grid in both windows.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [ChromeEarlGreyUI openTabGrid];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [ChromeEarlGreyUI openTabGrid];

  GREYWaitForAppToIdle(@"App failed to idle");

  // DnD first tab of left window to left edge of first tab in second window.
  // Note: move to left half of the destination tile, to avoid unwanted
  // scrolling that would happen closer to the left edge.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(0), 0,
                                            IdentifierForCellAtIndex(0), 1,
                                            CGVectorMake(0.5, 0.5)),
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

// Tests dragging all tab grid items to another window.
- (void)testDragAndDropAllItemsToOtherWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");
  }

  // Setup first window with tab 1 and 2.
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

  // Open tab grid in both windows.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [ChromeEarlGreyUI openTabGrid];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [ChromeEarlGreyUI openTabGrid];

  GREYWaitForAppToIdle(@"App failed to idle");

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridEditSelectAllButton()]
      performAction:grey_tap()];

  // DnD first tab of left window to left edge of first tab in second window.
  // Note: move to left half of the destination tile, to avoid unwanted
  // scrolling that would happen closer to the left edge.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(0), 0,
                                            IdentifierForCellAtIndex(0), 1,
                                            CGVectorMake(0.5, 0.5)),
             @"Failed to DND cell on cell");

  GREYWaitForAppToIdle(@"App failed to idle");

  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:4 inWindowWithNumber:1];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests dragging a selection of tab grid items between windows.
- (void)testDragAndDropSelectionBetweenWindows {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");
  }

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

  // Open tab grid in both windows.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [ChromeEarlGreyUI openTabGrid];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [ChromeEarlGreyUI openTabGrid];

  GREYWaitForAppToIdle(@"App failed to idle");

  // Enter Select mode.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SelectedStateTitleSelection(0)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap tab to select.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SelectedStateTitleSelection(1)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // DnD first tab of left window to left edge of first tab in second window.
  // Note: move to left half of the destination tile, to avoid unwanted
  // scrolling that would happen closer to the left edge.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(0), 0,
                                            IdentifierForCellAtIndex(0), 1,
                                            CGVectorMake(0.5, 0.5)),
             @"Failed to DND cell on cell");

  GREYWaitForAppToIdle(@"App failed to idle");

  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:3 inWindowWithNumber:1];

  // Check the original tab grid selection state title mentions no selection.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:SelectedStateTitleSelection(0)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Move third cell of second window as second cell in first window.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(2), 1,
                                            IdentifierForCellAtIndex(0), 0,
                                            CGVectorMake(1.0, 0.5)),
             @"Failed to DND cell on cell");

  GREYWaitForAppToIdle(@"App failed to idle");

  // Check the original tab grid selection state title still mentions no
  // selection.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:SelectedStateTitleSelection(0)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:1];

  // Exit Select mode.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

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
// TODO(crbug.com/40839724): Re-enable this test.
- (void)FLAKY_testDragAndDropIncognitoBetweenWindows {
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

  // Open tab grid in both windows.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [ChromeEarlGreyUI openTabGrid];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [ChromeEarlGreyUI openTabGrid];

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
// TODO(crbug.com/40864920): Re-enable this test.
- (void)DISABLED_testDragAndDropURLBetweenWindows {
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
  [ChromeEarlGreyUI openTabGrid];

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

  // TODO(crbug.com/40868899): Test is failing on iPad devices and simulator.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }

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
  [ChromeEarlGreyUI openTabGrid];
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
// TODO(crbug.com/40240640): Re-enable this test.
- (void)DISABLED_testDragAndDropMainURLInIncognitoWindow {
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
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [ChromeEarlGreyUI openTabGrid];

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

  [ChromeEarlGreyUI openTabGrid];

  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];

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

  [ChromeEarlGreyUI openTabGrid];

  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];

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

  [ChromeEarlGreyUI openTabGrid];

  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];

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
  [ChromeEarlGrey loadURL:_URL4];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse4];

  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [ChromeEarlGreyUI openTabGrid];

  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];

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

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_BOOKMARK_CHOOSE_GROUP_BUTTON))]
      assertWithMatcher:grey_notNil()];

  // Choose "Mobile Bookmarks" folder as the destination.
  // Duplicate matcher here instead of using +[BookmarkEarlGreyUI
  // openMobileBookmarks] in order to properly wait for the snackbar message.
  std::u16string pattern =
      l10n_util::GetStringUTF16(IDS_IOS_BOOKMARK_PAGE_SAVED_FOLDER);
  NSString* snackbarMessage = base::SysUTF16ToNSString(
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          pattern, "count", 2, "title",
          base::SysNSStringToUTF16(@"Mobile Bookmarks")));
  [self waitForSnackBarMessageText:snackbarMessage
      triggeredByTappingItemWithMatcher:grey_allOf(grey_kindOfClassName(
                                                       @"UITableViewCell"),
                                                   grey_descendant(grey_text(
                                                       @"Mobile Bookmarks")),
                                                   nil)];

  [BookmarkEarlGrey
      verifyExistenceOfBookmarkWithURL:base::SysUTF8ToNSString(_URL1.spec())
                                  name:kTitle1
                             inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      verifyExistenceOfBookmarkWithURL:base::SysUTF8ToNSString(_URL4.spec())
                                  name:kTitle4
                             inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      verifyAbsenceOfBookmarkWithURL:base::SysUTF8ToNSString(_URL2.spec())
                           inStorage:BookmarkStorageType::kLocalOrSyncable];
}

// Tests adding items to the Reading List from the tab grid edit mode.
// TODO(crbug.com/40900596): Test flakes.
- (void)FLAKY_testTabGridBulkActionAddToReadingList {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];

  [ChromeEarlGreyUI openTabGrid];

  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];

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
  // TODO(crbug.com/40193498): The pasteboard is "not available at this time"
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

  [ChromeEarlGreyUI openTabGrid];

  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];

  // Select the first and last items.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(2)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridEditShareButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey tapButtonInActivitySheetWithID:@"Copy"];

  NSString* URL1String = base::SysUTF8ToNSString(_URL1.spec());
  NSString* URL3String = base::SysUTF8ToNSString(_URL3.spec());

  [ChromeEarlGrey verifyStringCopied:URL1String];
  [ChromeEarlGrey verifyStringCopied:URL3String];
}

#pragma mark - Tab Grid Search

// Tests entering and exit of the tab grid search mode.
- (void)testEnterExitSearch {
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openTabGrid];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Verify that search mode is active.
  [[EarlGrey selectElementWithMatcher:TabGridSearchModeToolbar()]
      assertWithMatcher:grey_notNil()];

  // Exit search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchCancelButton()]
      performAction:grey_tap()];

  // Verify that normal mode is active.
  [[EarlGrey selectElementWithMatcher:TabGridNormalModePageControl()]
      assertWithMatcher:grey_notNil()];
}

// Tests that exiting search mode reset the tabs count to the original number.
- (void)testTabGridResetAfterExitingSearch {
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openTabGrid];

  // Enter search mode & search with a query that produce no results.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(@"hello")];

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
  [ChromeEarlGreyUI openTabGrid];

  // Make sure the tab grid is on the regular tabs panel.
  [[EarlGrey selectElementWithMatcher:TabGridNormalModePageControl()]
      performAction:grey_tap()];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Upon entry, the search bar is empty. Verify that scrim is visible.
  [[EarlGrey selectElementWithMatcher:VisibleSearchScrim()]
      assertWithMatcher:grey_notNil()];

  // Searching with any query should render scrim invisible.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(@"text")];
  [[EarlGrey selectElementWithMatcher:VisibleSearchScrim()]
      assertWithMatcher:grey_nil()];

  // Clearing search bar text should render scrim visible again.
  // TODO(crbug.com/40916973): Revert to grey_clearText when fixed in EG.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(@"")];
  [[EarlGrey selectElementWithMatcher:VisibleSearchScrim()]
      assertWithMatcher:grey_notNil()];

  // Cancel search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchCancelButton()]
      performAction:grey_tap()];

  // Verify that scrim is not visible anymore.
  [[EarlGrey selectElementWithMatcher:VisibleSearchScrim()]
      assertWithMatcher:grey_nil()];
}

// Tests that tapping on the scrim view while in search mode dismisses the scrim
// and exits search mode.
- (void)testTapOnSearchScrimExitsSearchMode {
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openTabGrid];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Tap on scrim.
  [[EarlGrey selectElementWithMatcher:VisibleSearchScrim()]
      performAction:grey_tap()];

  // Verify that search mode is exit, scrim not visible, and transition to
  // normal mode was successful.
  [[EarlGrey selectElementWithMatcher:VisibleSearchScrim()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:TabGridNormalModePageControl()]
      assertWithMatcher:grey_notNil()];
  [self verifyVisibleTabsCount:2];
}

// Tests that searching in open tabs in the regular mode will filter the tabs
// correctly.
// TODO(crbug.com/332714545): Test is flaky.
- (void)FLAKY_testSearchRegularOpenTabs {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGreyUI openTabGrid];

  [self verifyVisibleTabsCount:4];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(@"Page")];

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
      performAction:grey_replaceText(@"Foo")];

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
  [ChromeEarlGreyUI openTabGrid];

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
  PerformTabGridSearch(@"text");

  [[EarlGrey selectElementWithMatcher:SearchOpenTabsSectionHeader()]
      assertWithMatcher:grey_notNil()];

  // Clearing search bar text should render the header invisible again.
  // TODO(crbug.com/40916973): Revert to grey_clearText when fixed in EG.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(@"")];
  [[EarlGrey selectElementWithMatcher:SearchOpenTabsSectionHeader()]
      assertWithMatcher:grey_nil()];

  // Searching a word then canceling the search mode should hide the section
  // header.
  PerformTabGridSearch(@"page");

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
  [ChromeEarlGreyUI openTabGrid];

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
  PerformTabGridSearch(@"text");

  [[EarlGrey selectElementWithMatcher:SearchOpenTabsHeaderWithValue(0)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSectionHeader()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSection()]
      assertWithMatcher:grey_notNil()];

  // Clearing search bar text should hide the suggested actions section.
  // TODO(crbug.com/40916973): Revert to grey_clearText when fixed in EG.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(@"")];
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSectionHeader()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSection()]
      assertWithMatcher:grey_nil()];

  // Searching with a query with results should show the suggested actions
  // section.
  // TODO(crbug.com/40916973): Revert to grey_clearText when fixed in EG.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(@"")];
  PerformTabGridSearch(kTitle2);

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
  [ChromeEarlGreyUI openTabGrid];

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
  PerformTabGridSearch(kTitle2);

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
  [ChromeEarlGreyUI openTabGrid];

  // Enter search mode and enter a search query.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];
  // TODO(crbug.com/40827691): Scrolling doesn't work properly in very small
  // devices. Once that is fixed a more broad query can be used for searching
  // (eg. "page").
  PerformTabGridSearch(kTitle2);

  // Verify that the suggested actions section exist and has "Search on web",
  // "Search recent tabs", "Search history" rows.
  [[self scrollDownViewMatcher:RegularTabGrid()
               toSelectMatcher:SearchSuggestedActionsSectionHeader()]
      assertWithMatcher:grey_notNil()];

  id<GREYMatcher> recentTabsMatcher =
      grey_descendant(SearchRecentTabsSuggestedAction());
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    // When Tab Group Sync is enabled, Recent Tabs is not reachable from
    // Tab Grid.
    recentTabsMatcher = grey_not(recentTabsMatcher);
  }

  [[self
      scrollDownViewMatcher:RegularTabGrid()
            toSelectMatcher:grey_allOf(
                                SearchSuggestedActionsSection(),
                                grey_descendant(SearchOnWebSuggestedAction()),
                                recentTabsMatcher,
                                grey_descendant(SearchHistorySuggestedAction()),
                                grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the search suggested actions section has the right rows in the
// Recent Tabs page.
- (void)testSearchSuggestedActionsSectionContentInRecentTabs {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the test flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when Tab "
                           @"Group Sync is enabled.");
  }

  [self loadTestURLsInNewTabs];
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
  // Scroll all the way to the top of the Recent Tabs page because a prior
  // test may have left it partially scrolled down.
  [[EarlGrey selectElementWithMatcher:RecentTabsTable()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];

  // Enter search mode and enter a search query.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];
  // TODO(crbug.com/40827691): Scrolling doesn't work properly in very small
  // devices. Once that is fixed a more broad query can be used for searching
  // (eg. "page").
  PerformTabGridSearch(kTitle2);

  // Verify that the suggested actions section exist and has "Search on web",
  // "Search open tabs", "Search history" rows.
  [[self scrollDownViewMatcher:RecentTabsTable()
               toSelectMatcher:chrome_test_util::HeaderWithAccessibilityLabelId(
                                   IDS_IOS_TABS_SEARCH_SUGGESTED_ACTIONS)]
      assertWithMatcher:grey_notNil()];
  [[self scrollDownViewMatcher:RecentTabsTable()
               toSelectMatcher:SearchOnWebSuggestedAction()]
      assertWithMatcher:grey_notNil()];
  [[self scrollDownViewMatcher:RecentTabsTable()
               toSelectMatcher:SearchOpenTabsSuggestedAction()]
      assertWithMatcher:grey_notNil()];
  [[self scrollDownViewMatcher:RecentTabsTable()
               toSelectMatcher:SearchHistorySuggestedAction()]
      assertWithMatcher:grey_notNil()];
}

// Tests that history row in the search suggested actions section displays the
// correct number of matches.
- (void)testSearchSuggestedActionsDisplaysCorrectHistoryMatchesCount {
  [ChromeEarlGrey clearBrowsingHistory];
  [self loadTestURLs];
  [ChromeEarlGreyUI openTabGrid];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Verify that the suggested actions section is not visible.
  [[EarlGrey selectElementWithMatcher:SearchSuggestedActionsSection()]
      assertWithMatcher:grey_nil()];

  // Searching the word "page" matches 2 items from history.
  PerformTabGridSearch(@"page");

  [[self scrollDownViewMatcher:RegularTabGrid()
               toSelectMatcher:
                   SearchSuggestedActionsSectionWithHistoryMatchesCount(2)]
      assertWithMatcher:grey_notNil()];

  // Adding to the existing query " two" will search for "page two" and should
  // only match 1 item from the history.
  PerformTabGridSearch(@"page two");

  [[self scrollDownViewMatcher:RegularTabGrid()
               toSelectMatcher:
                   SearchSuggestedActionsSectionWithHistoryMatchesCount(1)]
      assertWithMatcher:grey_notNil()];

  // Cancel search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchCancelButton()]
      performAction:grey_tap()];
}

// Tests that history row in the search suggested actions section displays the
// correct number of matches in Recent Tabs.
- (void)testRecentTabsSearchSuggestedActionsDisplaysCorrectHistoryMatchesCount {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the test flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when Tab "
                           @"Group Sync is enabled.");
  }

  [ChromeEarlGrey clearBrowsingHistory];
  [self loadTestURLs];
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
  // Scroll all the way to the top of the recent tabs page because a prior
  // test may have left it partially scrolled down.
  [[EarlGrey selectElementWithMatcher:RecentTabsTable()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Verify that the suggested actions section does not exist.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HeaderWithAccessibilityLabelId(
                                   IDS_IOS_TABS_SEARCH_SUGGESTED_ACTIONS)]
      assertWithMatcher:grey_notVisible()];

  // Searching the word "page" matches 2 items from history.
  PerformTabGridSearch(@"page");

  [[self
      scrollDownViewMatcher:RecentTabsTable()
            toSelectMatcher:RecentTabsSearchHistorySuggestedActionWithMatches(
                                2)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Adding to the existing query " two" will search for "page two" and should
  // only match 1 item from the history.
  PerformTabGridSearch(@"page two");

  [[self
      scrollDownViewMatcher:RecentTabsTable()
            toSelectMatcher:RecentTabsSearchHistorySuggestedActionWithMatches(
                                1)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Cancel search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchCancelButton()]
      performAction:grey_tap()];
}

// Tests that selecting an open tab search result in the regular mode will
// correctly open the expected tab.
- (void)testSearchRegularOpenTabsSelectResult {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGreyUI openTabGrid];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(kTitle2)];

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
  [ChromeEarlGreyUI openTabGrid];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(kTitle2)];

  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTitle2)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];
  const GURL currentURL = [ChromeEarlGrey webStateVisibleURL];
  GREYAssertEqual(_URL2, currentURL, @"Page navigated unexpectedly to %s",
                  currentURL.spec().c_str());
}

// Tests that share action works successfully from the long press context menu
// on search results.
- (void)testSearchOpenTabsContextMenuShare {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGreyUI openTabGrid];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(kTitle2)];

  [self longPressTabWithTitle:kTitle2];

  [ChromeEarlGrey verifyShareActionWithURL:_URL1 pageTitle:kTitle2];
}

// Tests that add to reading list action works successfully from the long press
// context menu on search results.
- (void)testSearchOpenTabsContextMenuAddToReadingList {
  // Clear the Reading List.
  GREYAssertNil([ReadingListAppInterface clearEntries],
                @"Unable to clear Reading List entries");
  GREYAssertEqual(0, [ReadingListAppInterface unreadEntriesCount],
                  @"Reading List should be empty");

  [self loadTestURLsInNewTabs];
  [ChromeEarlGreyUI openTabGrid];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(kTitle2)];

  [self longPressTabWithTitle:kTitle2];

  [[EarlGrey selectElementWithMatcher:AddToReadingListButton()]
      performAction:grey_tap()];

  // Check that the tab was added to Reading List.
  GREYAssertEqual(1, [ReadingListAppInterface unreadEntriesCount],
                  @"Reading List should have one element");
}

// Tests that add to bookmarks action works successfully from the long press
// context menu on search results.
- (void)testSearchOpenTabsContextMenuAddToBookmarks {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGreyUI openTabGrid];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(kTitle2)];

  [self longPressTabWithTitle:kTitle2];

  NSString* snackbarMessage = base::SysUTF16ToNSString(
      l10n_util::GetPluralStringFUTF16(IDS_IOS_BOOKMARKS_BULK_SAVED, 1));
  [self waitForSnackBarMessageText:snackbarMessage
      triggeredByTappingItemWithMatcher:AddToBookmarksButton()];

  [self longPressTabWithTitle:kTitle2];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_TOOLS_MENU_EDIT_BOOKMARK)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
                     IDS_IOS_BOOKMARK_EDIT_SCREEN_TITLE)]
      assertWithMatcher:grey_notNil()];
}

// Tests that close tab action works successfully from the long press context
// menu on search results.
- (void)testSearchOpenTabsContextMenuCloseTab {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGreyUI openTabGrid];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(kTitle2)];

  [self longPressTabWithTitle:kTitle2];

  // Close Tab.
  [[EarlGrey selectElementWithMatcher:CloseTabMenuButton()]
      performAction:grey_tap()];

  // Make sure that the tab is no longer present.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTitle2)]
      assertWithMatcher:grey_nil()];
}

- (void)testSearchOpenTabsContextMenuSelectTabsUnavailable {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGreyUI openTabGrid];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(kTitle2)];

  [self longPressTabWithTitle:kTitle2];

  [[EarlGrey selectElementWithMatcher:SelectTabsContextMenuItem()]
      assertWithMatcher:grey_nil()];

  // Dismiss the context menu.
  [[EarlGrey selectElementWithMatcher:RegularTabGrid()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];
}

// Tests "search recent tabs" and "search open tabs" suggested actions switch
// the tab grid page correctly while staying in the search mode.
- (void)testSearchSuggestedActionsPageSwitch {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the test flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when Tab "
                           @"Group Sync is enabled.");
  }

  [self loadTestURLsInNewTabs];
  [ChromeEarlGreyUI openTabGrid];
  // Enter search mode & perform a search.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  PerformTabGridSearch(kTitle2);

  // Verify that the RegularGridView is visible, use 50% for the visibility
  // percentage as in smaller devices the toolbars can occupy more space on the
  // screen.
  [[EarlGrey selectElementWithMatcher:RegularTabGrid()]
      assertWithMatcher:grey_minimumVisiblePercent(0.5)];

  // Tap on search recent tabs.
  [[self scrollDownViewMatcher:RegularTabGrid()
               toSelectMatcher:SearchRecentTabsSuggestedAction()]
      performAction:grey_tap()];

  // Verify successful transition to the recent tabs page with search mode on
  // and search text set to the original query.
  [[EarlGrey selectElementWithMatcher:TabGridSearchModeToolbar()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:SearchBarWithSearchText(kTitle2)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:RecentTabsTable()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:RegularTabGrid()]
      assertWithMatcher:grey_notVisible()];

  // Tap on search open tabs to go back to regular tabs page.
  [[self scrollDownViewMatcher:RecentTabsTable()
               toSelectMatcher:SearchOpenTabsSuggestedAction()]
      performAction:grey_tap()];

  // Verify successful transition to the regular tab grid page with search mode
  // on and search text set to the original query.
  [[EarlGrey selectElementWithMatcher:TabGridSearchModeToolbar()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:SearchBarWithSearchText(kTitle2)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:RegularTabGrid()]
      assertWithMatcher:grey_minimumVisiblePercent(0.5)];
  [[EarlGrey selectElementWithMatcher:RecentTabsTable()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that tapping on search history action in the regular tabs search mode
// opens the history modal and dismissing it returns to the search mode.
- (void)testHistorySuggestedActionInRegularTabsSearch {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGreyUI openTabGrid];
  // Enter search mode & perform a search.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  PerformTabGridSearch(kTitle2);

  // Tap on search history.
  [[self scrollDownViewMatcher:RegularTabGrid()
               toSelectMatcher:SearchHistorySuggestedAction()]
      performAction:grey_tap()];

  // Check that the history table is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryTableViewIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Close History.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kHistoryNavigationControllerDoneButtonIdentifier)]
      performAction:grey_tap()];

  // Make sure that search mode is still active and searching the same query.
  [[EarlGrey selectElementWithMatcher:TabGridSearchModeToolbar()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:SearchBarWithSearchText(kTitle2)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tapping on search history action in the Recent Tabs search mode
// opens the history modal and dismissing it returns to the search mode.
- (void)testHistorySuggestedActionInRecentTabsSearch {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the test flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when Tab "
                           @"Group Sync is enabled.");
  }

  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
  // Scroll all the way to the top of the recent tabs page because a prior
  // test may have left it partially scrolled down.
  [[EarlGrey selectElementWithMatcher:RecentTabsTable()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];

  // Enter search mode & perform a search.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];
  PerformTabGridSearch(kTitle2);

  // Tap on search history.
  [[self scrollDownViewMatcher:RecentTabsTable()
               toSelectMatcher:SearchHistorySuggestedAction()]
      performAction:grey_tap()];

  // Check that the history table is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryTableViewIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Close History.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kHistoryNavigationControllerDoneButtonIdentifier)]
      performAction:grey_tap()];

  // Make sure that search mode is still active and searching the same query.
  [[EarlGrey selectElementWithMatcher:TabGridSearchModeToolbar()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:SearchBarWithSearchText(kTitle2)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tapping the search on web action in the regular tabs search mode
// opens a new tab on the default search engine with the search term from tab
// search. Additionally, checks that tab search mode is exited when the user
// returns to the tab grid.
- (void)testSearchOnWebSuggestedActionInRegularTabsSearch {
  // Configure a testing search engine to prevent real external url requests.
  web::test::AddResponseProvider(
      std::make_unique<EchoURLDefaultSearchEngineResponseProvider>());
  GURL searchEngineURL = web::test::HttpServer::MakeUrl(kSearchEngineURL);
  NSString* searchEngineURLString =
      base::SysUTF8ToNSString(searchEngineURL.spec());
  [SettingsAppInterface overrideSearchEngineWithURL:searchEngineURLString];

  // Enter tab grid search mode & perform a search.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  PerformTabGridSearch(@"queryfromtabsearch");

  // Scroll to search on web.
  [[self scrollDownViewMatcher:RegularTabGrid()
               toSelectMatcher:SearchOnWebSuggestedAction()]
      performAction:grey_tap()];

  // Ensure that the tab grid was exited.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Ensure the loaded page is a default search engine page for `searchQuery`.
  [ChromeEarlGrey waitForWebStateContainingText:kSearchEngineHost];
  [ChromeEarlGrey waitForWebStateContainingText:"queryfromtabsearch"];

  // Re-enter the tab grid and ensure search mode was exited.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tapping the search on web action in the Recent Tabs search mode
// opens a new tab on the default search engine with the search term from tab
// search. Additionally, checks that tab search mode is exited when the user
// returns to the tab grid.
- (void)testSearchOnWebSuggestedActionInRecentTabsSearch {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the test flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when Tab "
                           @"Group Sync is enabled.");
  }

  // Configure a testing search engine to prevent real external url requests.
  web::test::AddResponseProvider(
      std::make_unique<EchoURLDefaultSearchEngineResponseProvider>());
  GURL searchEngineURL = web::test::HttpServer::MakeUrl(kSearchEngineURL);
  NSString* searchEngineURLString =
      base::SysUTF8ToNSString(searchEngineURL.spec());
  [SettingsAppInterface overrideSearchEngineWithURL:searchEngineURLString];

  // Enter tab grid search mode & perform a search.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
  // Scroll all the way to the top of the recent tabs page because a prior
  // test may have left it partially scrolled down.
  [[EarlGrey selectElementWithMatcher:RecentTabsTable()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];

  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];
  PerformTabGridSearch(@"queryfromtabsearch");

  [[self scrollDownViewMatcher:RecentTabsTable()
               toSelectMatcher:SearchOnWebSuggestedAction()]
      performAction:grey_tap()];

  // Ensure that the tab grid was exited.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Ensure the loaded page is a default search engine page for `searchQuery`.
  [ChromeEarlGrey waitForWebStateContainingText:kSearchEngineHost];
  [ChromeEarlGrey waitForWebStateContainingText:"queryfromtabsearch"];

  // Re-enter the tab grid and ensure search mode was exited.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that a search with no results in incognito mode will show the empty
// state.
- (void)testEmptyStateAfterNoResultsSearchForIncognitoTabGrid {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];
  [ChromeEarlGreyUI openTabGrid];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Search with a word that will produce no results and verify that the header
  // has 0 found results and that the empty state is visible.
  PerformTabGridSearch(@"text");

  [[EarlGrey selectElementWithMatcher:SearchOpenTabsHeaderWithValue(0)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kTabGridIncognitoTabsEmptyStateIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that closing a tab works successfully in search results.
- (void)testSearchResultCloseTab {
  [self loadTestURLsInNewTabs];
  [ChromeEarlGreyUI openTabGrid];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  PerformTabGridSearch(kTitle2);

  [self verifyVisibleTabsCount:1];
  [[EarlGrey selectElementWithMatcher:TabWithTitleAndIndex(kTitle2, 0)]
      assertWithMatcher:grey_notNil()];

  // Close Tab.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];

  // Make sure that the tab is no longer present.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTitle2)]
      assertWithMatcher:grey_nil()];
}

// Tests that closing a tab works successfully in incognito search results.
- (void)testSearchResultCloseTabInIncognito {
  [self loadTestURLsInNewIncognitoTabs];
  [ChromeEarlGreyUI openTabGrid];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  PerformTabGridSearch(kTitle2);

  [self verifyVisibleTabsCount:1];
  [[EarlGrey selectElementWithMatcher:TabWithTitleAndIndex(kTitle2, 0)]
      assertWithMatcher:grey_notNil()];

  // Close Tab.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];

  // Make sure that the tab is no longer present.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTitle2)]
      assertWithMatcher:grey_nil()];
}

// Tests that searching in Recent Tabs will filter the items correctly.
- (void)testSearchRecentlyClosedTabs {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the test flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when Tab "
                           @"Group Sync is enabled.");
  }

  [self clearAllRecentlyClosedItems];
  [self loadTestURLsAndCloseTabs];

  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
  // Scroll all the way to the top of the recent tabs page because a prior
  // test may have left it partially scrolled down.
  [[EarlGrey selectElementWithMatcher:RecentTabsTable()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];

  // Ensure all recently closed tab entries are present.
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabWithTitle(kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabWithTitle(kTitle2)]
      assertWithMatcher:grey_notNil()];

  // Enter search mode and search for `kTitle2`.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(kTitle2)];

  // The recently closed section header should be visible.
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabsSectionHeader()]
      assertWithMatcher:grey_notNil()];

  // The item for page 2 should be displayed.
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabWithTitle(kTitle2)]
      assertWithMatcher:grey_notNil()];

  // The item for page 1 should not be visible.
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabWithTitle(kTitle1)]
      assertWithMatcher:grey_nil()];

  // Exit search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchCancelButton()]
      performAction:grey_tap()];

  // Check that all items are visible again.
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabWithTitle(kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabWithTitle(kTitle2)]
      assertWithMatcher:grey_notNil()];
}

// Tests that searching in Recent Tabs with no matching results hides the
// unmatched items and the "Recently Closed" section header.
- (void)testSearchRecentlyClosedTabsNoResults {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the test flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when Tab "
                           @"Group Sync is enabled.");
  }

  [self clearAllRecentlyClosedItems];
  [self loadTestURLsAndCloseTabs];

  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
  // Scroll all the way to the top of the recent tabs page because a prior
  // test may have left it partially scrolled down.
  [[EarlGrey selectElementWithMatcher:RecentTabsTable()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];

  // Ensure all recently closed tab entries are present.
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabWithTitle(kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabWithTitle(kTitle2)]
      assertWithMatcher:grey_notNil()];

  // Enter search mode and search for text which matches no items.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(@"foo")];

  // The recently closed section header should not be visible.
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabsSectionHeader()]
      assertWithMatcher:grey_nil()];

  // The items should not be visible.
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabWithTitle(kTitle1)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabWithTitle(kTitle2)]
      assertWithMatcher:grey_nil()];

  // Exit search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchCancelButton()]
      performAction:grey_tap()];

  // Check that all items are visible again.
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabWithTitle(kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabWithTitle(kTitle2)]
      assertWithMatcher:grey_notNil()];
}

// Tests that interacting with the Tab Grid search UI shows the correct header
// at each step.
- (void)testSearchHeaderWithInactiveTabs {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }
  [self loadTestURLsInNewTabs];
  [self relaunchAppWithInactiveTabsEnabled];

  [ChromeEarlGreyUI openTabGrid];
  GREYAssertEqual(1, [ChromeEarlGrey mainTabCount],
                  @"Expected only one tab (NTP), all other tabs should have "
                  @"been in inactive tab grid.");
  GREYAssertEqual(4, [ChromeEarlGrey inactiveTabCount],
                  @"Expected 4 inactive tabs.");

  // Verify that the Inactive Tabs button is showing.
  [[EarlGrey selectElementWithMatcher:TabGridInactiveTabsButton()]
      assertWithMatcher:grey_notNil()];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Verify that the Inactive Tabs button is not showing.
  [[EarlGrey selectElementWithMatcher:TabGridInactiveTabsButton()]
      assertWithMatcher:grey_nil()];

  // Verify that search mode is active.
  [[EarlGrey selectElementWithMatcher:TabGridSearchModeToolbar()]
      assertWithMatcher:grey_notNil()];

  // Verify that the Search results header is not showing, as the search field
  // is empty.
  [[EarlGrey selectElementWithMatcher:SearchOpenTabsSectionHeader()]
      assertWithMatcher:grey_nil()];

  // Enter some text.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(@"Page")];

  // Verify that the Search results header is showing now.
  [[EarlGrey selectElementWithMatcher:SearchOpenTabsSectionHeader()]
      assertWithMatcher:grey_notNil()];

  // Exit search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchCancelButton()]
      performAction:grey_tap()];

  // Verify that normal mode is active.
  [[EarlGrey selectElementWithMatcher:TabGridNormalModePageControl()]
      assertWithMatcher:grey_notNil()];

  // Verify that the Inactive Tabs button is showing.
  [[EarlGrey selectElementWithMatcher:TabGridInactiveTabsButton()]
      assertWithMatcher:grey_notNil()];

  // Verify that the Search results header is not showing, as the search field
  // is empty.
  [[EarlGrey selectElementWithMatcher:SearchOpenTabsSectionHeader()]
      assertWithMatcher:grey_nil()];
}

// Tests that once an account is signed in, the syncing spinner is eventually
// dismissed: https://crbug.com/1422634.
- (void)DISABLED_testSyncSpinnerDismissedInRecentlyClosedTabs {
  // Clear browsing history to reduce delay during sign-in and fix this test's
  // flakiness on iOS 16.
  [ChromeEarlGrey clearBrowsingHistory];

  // Sign-in with fake identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  // Open recently closed tabs.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];

  // Wait for the syncing view to disappear.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kTableViewActivityIndicatorHeaderFooterViewId)];
}

// Regression test for crbug.com/1474793. Tests the sign-in promo in "tabs from
// other devices" reacts accordingly if the user signs in via a different
// surface. More specifically: on tap the promo shouldn't offer the sign-in
// sheet but only the history opt-in.
- (void)testPromoInTabsFromOtherDevicesListensToSignin {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Other Devices is not reachable from the Tab Grid. So the test flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Other Devices is not available in Tab Grid when "
                           @"Tab Group Sync is enabled.");
  }

  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kRecentTabsTabSyncOffButtonAccessibilityIdentifier),
                     grey_accessibilityElement(), nil)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_HISTORY_SYNC_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that closing a tab works successfully in incognito search results.
- (void)testLastIncognitoTabCloses {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGreyUI openTabGrid];

  [self verifyVisibleTabsCount:1];
  [[EarlGrey selectElementWithMatcher:TabWithTitleAndIndex(kTitle1, 0)]
      assertWithMatcher:grey_notNil()];

  // Close Tab.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];

  // Make sure that the tab is no longer present.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTitle1)]
      assertWithMatcher:grey_nil()];
}

// Tests that "undo" is still possible after navigating to the "recently
// closed tabs" panel.
- (void)testClosedTabsAddedToRecentlyClosedTabsAfterConfirmation {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the test flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when Tab "
                           @"Group Sync is enabled.");
  }

  // Clear all recently closed tabs.
  [self clearAllRecentlyClosedItems];

  // Load some real URL (NTP is not added to recently closed tabs).
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

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

  // Navigate to the "recently closed" panel.
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];

  // Check that there are no "recently closed" tabs visible.
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabWithTitle(kTitle1)]
      assertWithMatcher:grey_nil()];

  // Navigate back to the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];

  // Tap Undo button
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridUndoCloseAllButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close all the tabs (again).
  [[EarlGrey selectElementWithMatcher:VisibleTabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridEditMenuCloseAllButton()]
      performAction:grey_tap()];

  // Open a new tab. This should result in closing the tab grid, which will
  // confirm the close operation and add the tabs to recently closed.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Navigate to the "recently closed" panel.
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];

  // Check that the tabs closed are now visible.
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabWithTitle(kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:RecentlyClosedTabWithTitle(kTitle2)]
      assertWithMatcher:grey_nil()];

  // Navigate back to the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
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

// Open and close 2 unique tabs.
- (void)loadTestURLsAndCloseTabs {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];
  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey closeCurrentTab];
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
// This should not be called when Tab Group Sync is enabled, as there is no
// Recent Tabs in Tab Grid.
- (void)prepareRecentTabWithURL:(const GURL&)URL
                       response:(const char*)response {
  GREYAssert(![ChromeEarlGrey isTabGroupSyncEnabled],
             @"Recent Tabs is not available in Tab Grid when Tab Group Sync is "
             @"enabled.");

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:response];

  // Close the tab, making it appear in Recent Tabs.
  [ChromeEarlGrey closeCurrentTab];

  // Switch over to Recent Tabs.
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];

  // Scroll all the way to the top of the recent tabs page because a prior
  // test may have left it partially scrolled down.
  [[EarlGrey selectElementWithMatcher:RecentTabsTable()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];
}

// Long press on the recent tab entry or the tab item in the tab grid with
// `title`.
- (void)longPressTabWithTitle:(NSString*)title {
  // The test page may be there multiple times.
  // Don't use -waitForUIElementToAppearWithMatcher here as it doesn't support
  // the atIndex:0 check for multiple elements.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(title),
                                            grey_sufficientlyVisible(), nil)]
        atIndex:0] assertWithMatcher:grey_notNil() error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kWaitForUIElementTimeout, condition),
             @"Tab did not appear.");

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
  [ChromeEarlGreyUI openTabGrid];
}

- (void)waitForSnackBarMessage:(int)messageIdentifier
    triggeredByTappingItemWithMatcher:(id<GREYMatcher>)matcher {
  NSString* snackBarLabel = l10n_util::GetNSStringWithFixup(messageIdentifier);
  WaitForSnackbarTriggeredByTappingItem(snackBarLabel, matcher);
}

- (void)waitForSnackBarMessageText:(NSString*)snackBarLabel
    triggeredByTappingItemWithMatcher:(id<GREYMatcher>)matcher {
  WaitForSnackbarTriggeredByTappingItem(snackBarLabel, matcher);
}

- (void)waitForContextMenuToDisappear {
  ConditionBlock wait_for_disappearance = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_kindOfClassName(
                                                @"_UIContextMenuContainerView"),
                                            grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_nil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kWaitForUIElementTimeout, wait_for_disappearance),
             @"Context menu did not disappear.");
}

// Verifies that the tab grid has exactly `expectedCount` tabs.
- (void)verifyVisibleTabsCount:(NSUInteger)expectedCount {
  // Verify that the cell # `expectedCount` exist.
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

// Returns an interaction that scrolls down on the view matched by `viewMatcher`
// to search for the given `matcher`.
- (id<GREYInteraction>)scrollDownViewMatcher:(id<GREYMatcher>)viewMatcher
                             toSelectMatcher:(id<GREYMatcher>)matcher {
  return [[EarlGrey selectElementWithMatcher:matcher]
         usingSearchAction:grey_scrollInDirectionWithStartPoint(
                               kGREYDirectionDown, /*amount=*/100,
                               /*xOriginStartPercentage=*/0.5,
                               /*yOriginStartPercentage=*/0.5)
      onElementWithMatcher:viewMatcher];
}

// Returns an interaction that scrolls up on the view matched by `viewMatcher`
// to search for the given `matcher`.
- (id<GREYInteraction>)scrollUpViewMatcher:(id<GREYMatcher>)viewMatcher
                           toSelectMatcher:(id<GREYMatcher>)matcher {
  return [[EarlGrey selectElementWithMatcher:matcher]
         usingSearchAction:grey_scrollInDirectionWithStartPoint(
                               kGREYDirectionUp, /*amount=*/100,
                               /*xOriginStartPercentage=*/0.5,
                               /*yOriginStartPercentage=*/0.5)
      onElementWithMatcher:viewMatcher];
}

// Ensures that all items are cleared out from the saved recently closed items.
- (void)clearAllRecentlyClosedItems {
  [ChromeEarlGrey clearBrowsingHistory];
  [RecentTabsAppInterface clearCollapsedListViewSectionStates];
}

// Relaunches the app with Inactive Tabs enabled.
- (void)relaunchAppWithInactiveTabsEnabled {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      "--enable-features=" + std::string(kTabInactivityThreshold.name) + ":" +
      kTabInactivityThresholdParameterName + "/" +
      kTabInactivityThresholdImmediateDemoParam);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

@end
