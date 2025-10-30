// Copyright 2025 The Chromium Authors
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
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/history/ui_bundled/history_ui_constants.h"
#import "ios/chrome/browser/menu/ui_bundled/menu_action_type.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/popup_menu_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

using base::test::ios::kWaitForClearBrowsingDataTimeout;
using chrome_test_util::BrowsingDataButtonMatcher;
using chrome_test_util::ClearBrowsingDataButton;
using chrome_test_util::ClearBrowsingDataView;
using chrome_test_util::DeleteButton;
using chrome_test_util::HistoryClearBrowsingDataButton;
using chrome_test_util::HistoryEntry;
using chrome_test_util::NavigationBarDoneButton;
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

}  // namespace

// History System tests.
@interface HistorySystemTestCase : ChromeTestCase {
  GURL _URL1;
  GURL _URL2;
  GURL _URL3;
}

// Loads three test URLs.
- (void)loadTestURLs;
// Inserts three test URLs into the History Service.
- (void)addTestURLsToHistory;
// Displays the history UI.
- (void)openHistoryPanel;

@end

@implementation HistorySystemTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.additional_args.push_back(std::string("--") +
                                   syncer::kSyncShortNudgeDelayForTest);
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

  if (![ChromeTestCase forceRestartAndWipe]) {
    [ChromeEarlGrey clearBrowsingHistory];
  }

  // Some tests rely on a clean state for the "Clear Browsing Data" settings
  // screen.
  [ChromeEarlGrey resetBrowsingDataPrefs];
}

- (void)tearDownHelper {
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
  [super tearDownHelper];
}

// From history, delets browsing data with the default values which is 15min
// time range and includes history.
- (void)deleteBrowsingDataFromHistory {
  [ChromeEarlGreyUI tapPrivacyMenuButton:HistoryClearBrowsingDataButton()];
  [ChromeEarlGreyUI tapClearBrowsingDataMenuButton:ClearBrowsingDataButton()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Wait for the browsing data button to disappear.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:BrowsingDataButtonMatcher()
                                     timeout:kWaitForClearBrowsingDataTimeout];
}

#pragma mark Tests

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

// Tests that searching a typed URL (after history sync is enabled and the URL
// is uploaded to the sync server) displays only entries matching the search
// term.
// TODO(crbug.com/437843552): Test is flaky on simulator. Reenable the test.
#if TARGET_OS_SIMULATOR
#define MAYBE_testSearchSyncedHistory FLAKY_testSearchSyncedHistory
#else
#define MAYBE_testSearchSyncedHistory testSearchSyncedHistory
#endif
- (void)MAYBE_testSearchSyncedHistory {
  // TODO(crbug.com/437314320): Re-enable the test on iOS26.
  if (base::ios::IsRunningOnIOS26OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.");
  }

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

// Tests clear browsing history.
- (void)testClearBrowsingHistorySwipeDownDismiss {
  [self addTestURLsToHistory];
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

  [self addTestURLsToHistory];
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

// TODO(crbug.com/446382453): Deflake the test.
- (void)FLAKY_testHistorySyncInMultiwindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  // Create history in first window.
  [self loadTestURLs];

  // Open history panel in a second window
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
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
  [ChromeEarlGrey closeWindowWithNumber:1];
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

- (void)addTestURLsToHistory {
  [ChromeEarlGrey addHistoryServiceTypedURL:_URL1];
  [ChromeEarlGrey setHistoryServiceTitle:kTitle1 forPage:_URL1];
  [ChromeEarlGrey addHistoryServiceTypedURL:_URL2];
  [ChromeEarlGrey setHistoryServiceTitle:kTitle2 forPage:_URL2];
  [ChromeEarlGrey addHistoryServiceTypedURL:_URL3];
}

- (void)openHistoryPanel {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::HistoryDestinationButton()];
}

- (void)openHistoryPanelInWindowWithNumber:(int)windowNumber {
  [ChromeEarlGreyUI openToolsMenuInWindowWithNumber:windowNumber];
  // TODO(crbug.com/249582361): Switch back to using `tapToolsMenuButton:`
  // helper if/when the issue checking visibility of SwiftUI views in
  // multiwindow is fixed.
  chrome_test_util::TapAtOffsetOf(kToolsMenuHistoryId, windowNumber,
                                  CGVectorMake(0.5, 0.5));
}

@end
