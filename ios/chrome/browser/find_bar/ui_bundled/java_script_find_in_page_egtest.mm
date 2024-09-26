// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/find_in_page/model/util.h"
#import "ios/chrome/browser/find_bar/ui_bundled/find_bar_constants.h"
#import "ios/chrome/browser/find_bar/ui_bundled/java_script_find_in_page_controller_app_interface.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Test web page content.
const std::string kFindInPageResponse = "Find in page. Find in page.";

// Test web page URL.
const std::string kFindInPageTestURL = "/findinpage.html";

// Response handler that serves a test page for Find in Page.
std::unique_ptr<net::test_server::HttpResponse> FindInPageTestPageHttpResponse(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kFindInPageTestURL) {
    return nullptr;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(kFindInPageResponse);
  http_response->set_content_type("text/html");
  return std::move(http_response);
}

}  // namespace

// Tests for JavaScript Find in Page.
@interface JavaScriptFindInPageTestCase : ChromeTestCase

// Opens Find in Page.
- (void)openFindInPage;
// Closes Find in page.
- (void)closeFindInPage;
// Types text into Find in page textfield.
- (void)typeFindInPageText:(NSString*)text;
// Matcher for find in page textfield.
- (id<GREYMatcher>)findInPageInputField;
// Asserts that there is a string "`resultIndex` of `resultCount`" present on
// screen. Waits for up to 2 seconds for this to happen.
- (void)assertResultStringIsResult:(int)resultIndex outOfTotal:(int)resultCount;
// Taps Next button in Find in page.
- (void)advanceToNextResult;
// Taps Previous button in Find in page.
- (void)advanceToPreviousResult;
// Navigates to `kFindInPageTestURL` and waits for the page to load.
- (void)navigateToTestPage;

@end

@implementation JavaScriptFindInPageTestCase

#pragma mark - XCTest.

// After setup, a page with `kFindInPageResponse` is displayed and Find In Page
// bar is opened.
- (void)setUp {
  [super setUp];

  // Disabled for iOS 16.1.1+.
  if (base::ios::IsRunningOnOrLater(16, 1, 1)) {
    return;
  }

  // Clear saved search term.
  [JavaScriptFindInPageControllerAppInterface clearSearchTerm];

  // Setup find in page test server.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&FindInPageTestPageHttpResponse));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  [self navigateToTestPage];

  // Open Find in Page view.
  [self openFindInPage];
}

- (void)tearDown {
  // Disabled for iOS 16.1.1+.
  if (base::ios::IsRunningOnOrLater(16, 1, 1)) {
    [super tearDown];
    return;
  }

  // Close find in page view.
  [self closeFindInPage];

  [super tearDown];
}

#pragma mark - Tests.

// Tests that find in page allows iteration between search results and displays
// correct number of results.
- (void)testFindInPage {
  // Disabled for iOS 16.1.1+.
  if (base::ios::IsRunningOnOrLater(16, 1, 1)) {
    return;
  }
  // Type "find".
  [self typeFindInPageText:@"find"];
  // Should be highlighting result 1 of 2.
  [self assertResultStringIsResult:1 outOfTotal:2];
  // Tap Next.
  [self advanceToNextResult];
  // Should now read "2 of 2".
  [self assertResultStringIsResult:2 outOfTotal:2];
  // Go to previous.
  [self advanceToPreviousResult];
  [self assertResultStringIsResult:1 outOfTotal:2];
}

// Tests that Find In Page search term retention is working as expected, e.g.
// the search term is persisted between FIP runs, but in incognito search term
// is not retained and not autofilled.
- (void)testFindInPageRetainsSearchTerm {
  // Disabled for iOS 16.1.1+.
  if (base::ios::IsRunningOnOrLater(16, 1, 1)) {
    return;
  }
  // Type "find".
  [self typeFindInPageText:@"find"];
  [self assertResultStringIsResult:1 outOfTotal:2];
  [self closeFindInPage];

  // Verify it's closed.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kToolbarAccessoryContainerViewID)]
        assertWithMatcher:grey_nil()
                    error:&error];
    return (error == nil);
  };
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(2), condition),
      @"Timeout while waiting for Find Bar to close");

  // Open incognito page.
  [ChromeEarlGreyUI openNewIncognitoTab];
  [self navigateToTestPage];
  [self openFindInPage];
  // Check that no search term is prefilled.
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      assertWithMatcher:grey_text(@"")];
  [self typeFindInPageText:@"in"];
  [self assertResultStringIsResult:1 outOfTotal:4];
  [self closeFindInPage];

  // Navigate to a new non-incognito tab.
  [ChromeEarlGreyUI openNewTab];
  [self navigateToTestPage];
  [self openFindInPage];
  // Check that search term is retained from normal tab, not incognito tab.
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      assertWithMatcher:grey_text(@"find")];
  [self assertResultStringIsResult:1 outOfTotal:2];
}

// Tests accessibility of the Find in Page screen.
- (void)testAccessibilityOnFindInPage {
  // Disabled for iOS 16.1.1+.
  if (base::ios::IsRunningOnOrLater(16, 1, 1)) {
    return;
  }
  [self typeFindInPageText:@"find"];
  [self assertResultStringIsResult:1 outOfTotal:2];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

#pragma mark - Steps.

- (void)openFindInPage {
  [ChromeEarlGreyUI openToolsMenu];

  id<GREYMatcher> tableViewMatcher =
      [ChromeEarlGrey isNewOverflowMenuEnabled]
          ? grey_accessibilityID(kPopupMenuToolsMenuActionListId)
          : grey_accessibilityID(kPopupMenuToolsMenuTableViewId);
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kToolsMenuFindInPageId),
                                   grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 250)
      onElementWithMatcher:tableViewMatcher] performAction:grey_tap()];
}

- (void)closeFindInPage {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFindInPageCloseButtonId)]
      performAction:grey_tap()];
}

- (void)typeFindInPageText:(NSString*)text {
  chrome_test_util::TypeText(kFindInPageInputFieldId, 0, text);
  [ChromeEarlGreyUI waitForAppToIdle];
}

- (id<GREYMatcher>)findInPageInputField {
  return grey_accessibilityID(kFindInPageInputFieldId);
}

- (void)assertResultStringIsResult:(int)resultIndex
                        outOfTotal:(int)resultCount {
  // Returns "<current> of <total>" search results label (e.g "1 of 5").
  NSString* expectedResultsString = l10n_util::GetNSStringF(
      IDS_FIND_IN_PAGE_COUNT, base::NumberToString16(resultIndex),
      base::NumberToString16(resultCount));

  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(expectedResultsString)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return (error == nil);
  };
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(2), condition),
      @"Timeout waiting for correct Find in Page results string to appear");
}

- (void)advanceToNextResult {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFindInPageNextButtonId)]
      performAction:grey_tap()];
}

- (void)advanceToPreviousResult {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFindInPagePreviousButtonId)]
      performAction:grey_tap()];
}

- (void)navigateToTestPage {
  // Navigate to a page with some text.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kFindInPageTestURL)];

  // Verify web page finished loading.
  [ChromeEarlGrey waitForWebStateContainingText:kFindInPageResponse];
}

@end
