// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/strings/string_number_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/find_in_page/find_in_page_controller.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_constants.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_controller_ios.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Test web page content.
const std::string kFindInPageResponse = "Find in page. Find in page.";

}  // namespace

// Tests for Find in Page.
@interface FindInPageTestCase : ChromeTestCase

// URL for a test page with |kFindInPageResponse|.
@property(nonatomic, assign) GURL testURL;

// Opens Find in Page.
- (void)openFindInPage;
// Closes Find in page.
- (void)closeFindInPage;
// Types text into Find in page textfield.
- (void)typeFindInPageText:(NSString*)text;
// Matcher for find in page textfield.
- (id<GREYMatcher>)findInPageInputField;
// Asserts that there is a string "|resultIndex| of |resultCount|" present on
// screen. Waits for up to 2 seconds for this to happen.
- (void)assertResultStringIsResult:(int)resultIndex outOfTotal:(int)resultCount;
// Taps Next button in Find in page.
- (void)advanceToNextResult;
// Taps Previous button in Find in page.
- (void)advanceToPreviousResult;
// Navigates to |self.testURL| and waits for the page to load.
- (void)navigateToTestPage;

@end

@implementation FindInPageTestCase
@synthesize testURL = _testURL;

#pragma mark - XCTest.

// After setup, a page with |kFindInPageResponse| is displayed and Find In Page
// bar is opened.
- (void)setUp {
  [super setUp];
  // Clear saved search term
  [FindInPageController setSearchTerm:nil];

  // Setup find in page test URL.
  std::map<GURL, std::string> responses;
  self.testURL = web::test::HttpServer::MakeUrl("http://findinpage");
  responses[self.testURL] = kFindInPageResponse;
  web::test::SetUpSimpleHttpServer(responses);

  [self navigateToTestPage];

  // Open Find in Page view.
  [self openFindInPage];
}

- (void)tearDown {
  // Close find in page view.
  [self closeFindInPage];

  [super tearDown];
}

#pragma mark - Tests.

// Tests that find in page allows iteration between search results and displays
// correct number of results.
- (void)testFindInPage {
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
  // Type "find".
  [self typeFindInPageText:@"find"];
  [self assertResultStringIsResult:1 outOfTotal:2];
  [self closeFindInPage];

  // Verify it's closed.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kFindInPageContainerViewId)]
        assertWithMatcher:grey_nil()
                    error:&error];
    return (error == nil);
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(2.0, condition),
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
  [self typeFindInPageText:@"find"];

  // Wait for UI to finish loading screen, before programatically verifying
  // accessibility.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

#pragma mark - Steps.

- (void)openFindInPage {
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kToolsMenuFindInPageId),
                                   grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(kPopupMenuToolsMenuTableViewId)]
      performAction:grey_tap()];
}

- (void)closeFindInPage {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFindInPageCloseButtonId)]
      performAction:grey_tap()];
}

- (void)typeFindInPageText:(NSString*)text {
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      performAction:grey_replaceText(text)];
}

- (id<GREYMatcher>)findInPageInputField {
  return grey_accessibilityID(kFindInPageInputFieldId);
}

- (void)assertResultStringIsResult:(int)resultIndex
                        outOfTotal:(int)resultCount {
  // Returns "<current> of <total>" search results label (e.g "1 of 5").
  NSString* expectedResultsString = l10n_util::GetNSStringF(
      IDS_FIND_IN_PAGE_COUNT, base::IntToString16(resultIndex),
      base::IntToString16(resultCount));

  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(expectedResultsString)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return (error == nil);
  };
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(2.0, condition),
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
  [ChromeEarlGrey loadURL:self.testURL];

  // Verify web page finished loading.
  [ChromeEarlGrey waitForWebViewContainingText:kFindInPageResponse];
}

@end
