// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/sync/base/command_line_switches.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/history/ui_bundled/history_ui_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/menu/menu_action_type.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

using chrome_test_util::BrowsingDataButtonMatcher;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ClearBrowsingDataButton;
using chrome_test_util::ClearBrowsingDataView;
using chrome_test_util::ContainsPartialText;
using chrome_test_util::DeleteButton;
using chrome_test_util::HistoryClearBrowsingDataButton;
using chrome_test_util::HistoryEntry;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::OpenLinkInNewWindowButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::WindowWithNumber;

namespace {
char kURL1[] = "/firstURL";
char kURL2[] = "/secondURL";
char kURL3[] = "/thirdURL";
char kTitle1[] = "Page 1";
char kTitle2[] = "Page 2";
char kResponse1[] = "Test Page 1 content";
char kResponse2[] = "Test Page 2 content";
char kResponse3[] = "Test Page 3 content";

// Constant for timeout while waiting for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(10);

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
// Matcher for the cancel button.
id<GREYMatcher> CancelButton() {
  return grey_accessibilityID(kHistoryToolbarCancelButtonIdentifier);
}
// Matcher for the empty TableView illustrated background
id<GREYMatcher> EmptyIllustratedTableViewBackground() {
  return grey_accessibilityID(kTableViewIllustratedEmptyViewID);
}

// Provides responses for URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  const char kPageFormat[] = "<head><title>%s</title></head><body>%s</body>";
  if (request.relative_url == kURL1) {
    std::string page_html =
        base::StringPrintf(kPageFormat, kTitle1, kResponse1);
    http_response->set_content(page_html);
  } else if (request.relative_url == kURL2) {
    std::string page_html =
        base::StringPrintf(kPageFormat, kTitle2, kResponse2);
    http_response->set_content(page_html);
  } else if (request.relative_url == kURL3) {
    http_response->set_content(
        base::StringPrintf("<body>%s</body>", kResponse3));
  } else {
    return nullptr;
  }

  return std::move(http_response);
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
@interface HistoryUITestCase : ChromeTestCase {
  GURL _URL1;
  GURL _URL2;
  GURL _URL3;
}

// Loads three test URLs.
- (void)loadTestURLs;
// Displays the history UI.
- (void)openHistoryPanel;

@end

@implementation HistoryUITestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.additional_args.push_back(std::string("--") +
                                   syncer::kSyncShortNudgeDelayForTest);
  config.features_enabled.push_back(kIOSQuickDelete);
  return config;
}

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  _URL1 = self.testServer->GetURL(kURL1);
  _URL2 = self.testServer->GetURL(kURL2);
  _URL3 = self.testServer->GetURL(kURL3);

  [ChromeEarlGrey clearBrowsingHistory];
  // Some tests rely on a clean state for the "Clear Browsing Data" settings
  // screen.
  [ChromeEarlGrey resetBrowsingDataPrefs];

  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
}

- (void)tearDown {
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");

  NSError* error = nil;
  // Dismiss search bar by pressing cancel, if present. Passing error prevents
  // failure if the element is not found.
  [[EarlGrey selectElementWithMatcher:CancelButton()] performAction:grey_tap()
                                                              error:&error];
  // Dismiss history panel by pressing done, if present. Passing error prevents
  // failure if the element is not found.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()
              error:&error];

  // Some tests change the default values for the "Clear Browsing Data" settings
  // screen.
  [ChromeEarlGrey resetBrowsingDataPrefs];

  // Shutdown network process after tests run to avoid hanging from
  // clearing browsing history.
  [ChromeEarlGrey killWebKitNetworkProcess];
  [super tearDown];
}

// From history, delets browsing data with the default values which is 15min
// time range and includes history.
- (void)deleteBrowsingDataFromHistory {
  [ChromeEarlGreyUI tapPrivacyMenuButton:HistoryClearBrowsingDataButton()];
  [ChromeEarlGreyUI tapClearBrowsingDataMenuButton:ClearBrowsingDataButton()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Wait for the browsing data button to disappear.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:BrowsingDataButtonMatcher()];
}

#pragma mark Tests

// Tests that no history is shown if there has been no navigation.
- (void)testDisplayNoHistory {
  [self openHistoryPanel];
  [ChromeEarlGreyUI assertHistoryHasNoEntries];
}

// Tests that the history panel displays navigation history.
- (void)testDisplayHistory {
  [self loadTestURLs];
  [self openHistoryPanel];

  // Assert that history displays three entries.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL2)),
              kTitle2)] assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL3)),
              _URL3.GetContent())] assertWithMatcher:grey_notNil()];

  // Tap a history entry and assert that navigation to that entry's URL occurs.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];
}

// Tests that history is not changed after performing back navigation.
- (void)testHistoryUpdateAfterBackNavigation {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey loadURL:_URL2];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [self openHistoryPanel];

  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL2)),
              kTitle2)] assertWithMatcher:grey_notNil()];
}

// Tests that searching history displays only entries matching the search term.
- (void)testSearchHistory {
  [self loadTestURLs];
  [self openHistoryPanel];
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

  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL2)),
              kTitle2)] assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL3)),
              _URL3.GetContent())] assertWithMatcher:grey_nil()];
}

// Tests that searching a typed URL (after history sync is enabled and the URL
// is uploaded to the sync server) displays only entries matching the search
// term.
- (void)testSearchSyncedHistory {
  const char syncedURL[] = "http://mockurl/sync/";
  const GURL mockURL(syncedURL);

  // Sign in and enable history sync.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableHistorySync:YES];

  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  // Add a typed URL and wait for it to show up on the server.
  [ChromeEarlGrey addHistoryServiceTypedURL:mockURL];
    NSArray<NSURL*>* URLs = @[
      net::NSURLWithGURL(mockURL),
    ];
    [ChromeEarlGrey waitForSyncServerHistoryURLs:URLs
                                         timeout:kSyncOperationTimeout];

  [self loadTestURLs];
  [self openHistoryPanel];

  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];

  // Verify that scrim is visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistorySearchScrimIdentifier)]
      assertWithMatcher:grey_notNil()];

  NSString* searchString = base::SysUTF8ToNSString(mockURL.spec().c_str());

  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(searchString)];

  // Verify that scrim is not visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistorySearchScrimIdentifier)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(
                         @"mockurl/sync/"),
                     grey_ancestor(grey_kindOfClassName(@"TableViewURLCell")),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL2)),
              kTitle2)] assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL3)),
              _URL3.GetContent())] assertWithMatcher:grey_nil()];
}

// Tests that long press on scrim while search box is enabled dismisses the
// search controller.
- (void)testSearchLongPressOnScrimCancelsSearchController {
  [self loadTestURLs];
  [self openHistoryPanel];
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];

  // Try long press.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] performAction:grey_longPress()];

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
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL2)),
              kTitle2)] assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL3)),
              _URL3.GetContent())] assertWithMatcher:grey_notNil()];
}

// Tests deletion of history entries.
- (void)testDeleteHistory {
  // Assert that the DeleteBrowsingData histogram is empty at the beginning of
  // the test.
  ExpectDeleteBrowsingDataHistoryHistogram(0);

  [self loadTestURLs];
  [self openHistoryPanel];

  // Assert that three history elements are present.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL2)),
              kTitle2)] assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL3)),
              _URL3.GetContent())] assertWithMatcher:grey_notNil()];

  // Enter edit mode, select a history element, and press delete.
  [[EarlGrey selectElementWithMatcher:NavigationEditButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:DeleteHistoryEntriesButton()]
      performAction:grey_tap()];

  // Assert that the deleted entry is gone and the other two remain.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL2)),
              kTitle2)] assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL3)),
              _URL3.GetContent())] assertWithMatcher:grey_notNil()];

  // Assert that the DeleteBrowsingData histogram contains one bucket after one
  // deletion was requested.
  ExpectDeleteBrowsingDataHistoryHistogram(1);

  // Enter edit mode, select both remaining entries, and press delete.
  [[EarlGrey selectElementWithMatcher:NavigationEditButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL2)),
              kTitle2)] performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL3)),
              _URL3.GetContent())] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:DeleteHistoryEntriesButton()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI assertHistoryHasNoEntries];

  // Assert that the DeleteBrowsingData histogram contains two bucket after the
  // second deletion was requested.
  ExpectDeleteBrowsingDataHistoryHistogram(2);
}

// Tests clear browsing history.
// TODO(crbug.com/40888582): Fix flakiness.
- (void)DISABLED_testClearBrowsingHistory {
  [self loadTestURLs];
  [self openHistoryPanel];

  [ChromeEarlGreyUI openAndClearBrowsingDataFromHistory];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      grey_accessibilityID(kHistoryTableViewIdentifier)];
  [ChromeEarlGreyUI assertHistoryHasNoEntries];
}

// Tests clear browsing history.
- (void)testClearBrowsingHistorySwipeDownDismiss {
  [self loadTestURLs];
  [self openHistoryPanel];

  // Open Clear Browsing Data
  [[EarlGrey selectElementWithMatcher:HistoryClearBrowsingDataButton()]
      performAction:grey_tap()];

  // Check that the TableView is presented.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the TableView has been dismissed.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      assertWithMatcher:grey_nil()];
}

// Tests display and selection of 'Open in New Tab' in a context menu on a
// history entry.
- (void)testContextMenuOpenInNewTab {
  // At the beginning of the test, the Context Menu History Entry Actions metric
  // should be empty.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::OpenInNewTab);

  [self loadTestURLs];
  [self openHistoryPanel];

  // Long press on the history element.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] performAction:grey_longPress()];

  // Select "Open in New Tab" and confirm that new tab is opened with selected
  // URL.
  [ChromeEarlGrey verifyOpenInNewTabActionWithURL:_URL1.GetContent()];

  // Assert that the Context Menu History Entry Actions metric is populated.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::OpenInNewTab);
}

// Tests display and selection of 'Open in New Window' in a context menu on a
// history entry.
- (void)testContextMenuOpenInNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  // At the beginning of the test, the Context Menu History Entry Actions metric
  // should be empty.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::OpenInNewWindow);

  [self loadTestURLs];
  [self openHistoryPanel];

  // Long press on the history element.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] performAction:grey_longPress()];

  [ChromeEarlGrey verifyOpenInNewWindowActionWithContent:kResponse1];

  // Assert that the Context Menu History Entry Actions metric is populated.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::OpenInNewWindow);
}

// Tests display and selection of 'Open in New Incognito Tab' in a context menu
// on a history entry.
- (void)testContextMenuOpenInNewIncognitoTab {
  // At the beginning of the test, the Context Menu History Entry Actions metric
  // should be empty.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::OpenInNewIncognitoTab);

  [self loadTestURLs];
  [self openHistoryPanel];

  // Long press on the history element.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] performAction:grey_longPress()];

  // Select "Open in New Incognito Tab" and confirm that new tab is opened in
  // incognito with the selected URL.
  [ChromeEarlGrey verifyOpenInIncognitoActionWithURL:_URL1.GetContent()];

  // Assert that the Context Menu History Entry Actions metric is populated.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::OpenInNewIncognitoTab);
}

// Tests display and selection of 'Copy URL' in a context menu on a history
// entry.
- (void)testContextMenuCopy {
  // At the beginning of the test, the Context Menu History Entry Actions metric
  // should be empty.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::CopyURL);

  [self loadTestURLs];
  [self openHistoryPanel];

  // Long press on the history element.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] performAction:grey_longPress()];

  // Tap "Copy URL" and wait for the URL to be copied to the pasteboard.
  [ChromeEarlGrey
      verifyCopyLinkActionWithText:[NSString
                                       stringWithUTF8String:_URL1.spec()
                                                                .c_str()]];

  // Assert that the Context Menu History Entry Actions metric is populated.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::CopyURL);
}

// Tests display and selection of "Share" in the context menu for a history
// entry.
- (void)testContextMenuShare {
  // At the beginning of the test, the Context Menu History Entry Actions metric
  // should be empty.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/0, /*action=*/MenuActionType::Share);

  [self loadTestURLs];
  [self openHistoryPanel];

  // Long press on the history element.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] performAction:grey_longPress()];

  [ChromeEarlGrey
      verifyShareActionWithURL:_URL1
                     pageTitle:[NSString stringWithUTF8String:kTitle1]];

  // Assert that the Context Menu History Entry Actions metric is populated.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::Share);
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

  [self loadTestURLs];
  [self openHistoryPanel];

  // Long press on the history element.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:DeleteButton()] performAction:grey_tap()];

  // Assert that the deleted entry is gone and the other two remain.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] assertWithMatcher:grey_nil()];

  // Wait for the animations to be done, then validate.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL2)),
              kTitle2)];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL2)),
              kTitle2)] assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL3)),
              _URL3.GetContent())] assertWithMatcher:grey_notNil()];

  // Assert that the DeleteBrowsingData histogram contains one bucket after one
  // deletion was requested.
  ExpectDeleteBrowsingDataHistoryHistogram(1);
  // Assert that the Context Menu History Entry Actions metric is populated.
  ExpectContextMenuHistoryEntryActionsHistogram(
      /*count=*/1, /*action=*/MenuActionType::Delete);
}

// Tests that the VC can be dismissed by swiping down.
- (void)testSwipeDownDismiss {
  [self loadTestURLs];
  [self openHistoryPanel];

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
}

// Tests that the VC can be dismissed by swiping down while its searching.
- (void)testSwipeDownDismissWhileSearching {
// TODO(crbug.com/40689184): Test fails on iOS 13+ iPad devices.
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"This test fails on iOS 13+ iPad device.");
  }
#endif

  [self loadTestURLs];
  [self openHistoryPanel];

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
}

// Navigates to history and checks elements for accessibility.
- (void)testAccessibilityOnHistory {
  [self loadTestURLs];
  [self openHistoryPanel];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      grey_accessibilityID(kHistoryTableViewIdentifier)];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  // Close history.
    id<GREYMatcher> exitMatcher =
        grey_accessibilityID(kHistoryNavigationControllerDoneButtonIdentifier);
    [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];
}

// Tests that if only some of the history entries are deleted from Delete
// Browsing Data, then the history view is updated to reflect those deletions.
- (void)testPartialDeletion {
  const char olderURLString[] = "https://example.com";
  const GURL olderURL = GURL(olderURLString);

  // Reset all prefs, so they're at their default value.
  [ChromeEarlGrey resetBrowsingDataPrefs];

  // Disable closing tabs as it's on by default in delete browsing data, so the
  // tab closure animation is not run in iPads. This is needed so the history UI
  // is not closed due to the animation.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kCloseTabs];

  // Create an history entry that took place one day ago.
  const base::Time oneDayAgo = base::Time::Now() - base::Days(1);
  [ChromeEarlGrey addHistoryServiceTypedURL:olderURL visitTimestamp:oneDayAgo];

  // Create a recent history entry.
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  // Open history and delete browsing data with the default configuration.
  [self openHistoryPanel];
  [self deleteBrowsingDataFromHistory];

  // Check that the day old URL is still present.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          olderURL)),
              olderURLString)] assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the more recent visit is gone.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] assertWithMatcher:grey_nil()];
}

// Tests that if all history entries are deleted from Delete Browsing Data, that
// then the history view is updated to show the empty state.
- (void)testEmptyState {
  // Disable closing tabs as it's on by default in delete browsing data, so the
  // tab closure animation is not run in iPads. This is needed so the history UI
  // is not closed due to the animation.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kCloseTabs];

  [self loadTestURLs];
  [self openHistoryPanel];

  // The toolbar should contain the CBD and edit buttons.
  [[EarlGrey selectElementWithMatcher:HistoryClearBrowsingDataButton()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:NavigationEditButton()]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGreyUI openAndClearBrowsingDataFromHistory];

  // Toolbar should only contain CBD button and the background should contain
  // the Illustrated empty view
  [[EarlGrey selectElementWithMatcher:HistoryClearBrowsingDataButton()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:NavigationEditButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:EmptyIllustratedTableViewBackground()]
      assertWithMatcher:grey_notNil()];
}

#pragma mark Multiwindow

- (void)testHistorySyncInMultiwindow {
  // TODO(crbug.com/40198758): Test is flaky on iPad devices.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"This test is flaky on iPad devices.");
  }

  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  // Create history in first window.
  [self loadTestURLs];

  // Open history panel in a second window
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  [self openHistoryPanelInWindowWithNumber:1];

  // Assert that three history elements are present in second window.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL2)),
              kTitle2)] assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL3)),
              _URL3.GetContent())] assertWithMatcher:grey_notNil()];

  // Open history panel in first window also.
  [self openHistoryPanelInWindowWithNumber:0];

  // Assert that three history elements are present in first window.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL2)),
              kTitle2)] assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL3)),
              _URL3.GetContent())] assertWithMatcher:grey_notNil()];

  // Delete item 1 from first window.
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:DeleteButton()] performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] assertWithMatcher:grey_nil()];

  // And make sure it has disappeared from second window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey
      selectElementWithMatcher:
          HistoryEntry(
              base::UTF16ToUTF8(
                  url_formatter::
                      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                          _URL1)),
              kTitle1)] assertWithMatcher:grey_nil()];
}

#pragma mark Helper Methods

- (void)loadTestURLs {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];
}

- (void)openHistoryPanel {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::HistoryDestinationButton()];
}

- (void)openHistoryPanelInWindowWithNumber:(int)windowNumber {
  [ChromeEarlGreyUI openToolsMenuInWindowWithNumber:windowNumber];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::HistoryDestinationButton()];
}

@end
