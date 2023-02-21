// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/find_in_page_egtest.h"

#import <XCTest/XCTest.h>

#import <sstream>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/find_in_page/features.h"
#import "ios/chrome/browser/find_in_page/find_in_page_app_interface.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kFindInPageTestRepeatingText[] = "repeating";
const char kFindInPageTestShortTextID[] = "shortText";
const char kFindInPageTestShortText[] = "ShortQuery";
const char kFindInPageTestLongText[] =
    "This is a particularly long string with a great number of characters";
const char kFindInPageTestSpecialCharactersText[] = "!@#$%^&*()_+";
const char kFindInPageTestNumbersText[] = "1234567890";
const char kFindInPageTestAlphanumericText[] = "f00bar";
const char kFindInPageTestNonASCIIText[] = "大家好🦑";
const char kFindInPageTestWithSpanishAccentText[] = "á";
const char kFindInPageTestWithoutSpanishAccentText[] = "a";
const char kFindInPageTestLowercaseAndUppercaseText[] =
    "ThIs tExT Is bOtH UpPeRcAsE AnD LoWeRcAsE";
const char kFindInPageTestRTLText[] = "He said \"שלם\" (shalom] to me.";

const char kFindInPageTestURL[] = "/findinpage.html";
const char kFindInPageCrossOriginFrameTestURL[] = "/crossorigin.html";
const char kFindInPageComplexPDFTestURL[] = "/complex_document.pdf";

namespace {

// Returns the test content for different test cases.
std::string FindInPageTestContent() {
  std::ostringstream oss;
  oss << "<div>";
  oss << "Text that repeats: " << kFindInPageTestRepeatingText
      << kFindInPageTestRepeatingText << "</p>";
  oss << "  <p id=\"" << kFindInPageTestShortTextID << "\">"
      << kFindInPageTestShortText << "</p>";
  oss << "  <p>" << kFindInPageTestLongText << "</p>";
  oss << "  <p>Special characters: " << kFindInPageTestSpecialCharactersText
      << "</p>";
  oss << "  <p>Numbers: " << kFindInPageTestNumbersText << "</p>";
  oss << "  <p>Alphanumeric text: " << kFindInPageTestAlphanumericText
      << "</p>";
  oss << "  <p>Non-ASCII text: " << kFindInPageTestNonASCIIText << "</p>";
  oss << "  <p>Text without spanish accent: "
      << kFindInPageTestWithoutSpanishAccentText << "</p>";
  oss << "  <p>Case sensitivity: " << kFindInPageTestLowercaseAndUppercaseText
      << "</p>";
  oss << "  <p dir=\"RTL\">" << kFindInPageTestRTLText << "</p>";
  oss << "  <div>";
  oss << "</div>";
  return oss.str();
}

// Response handler that serves a test page for Find in Page.
std::unique_ptr<net::test_server::HttpResponse> FindInPageTestPageHttpResponse(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kFindInPageTestURL) {
    return nullptr;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(
      "<html><head><meta charset=\"UTF-8\"></head><body>" +
      FindInPageTestContent() + "</body></html>");
  return std::move(http_response);
}

// Response handler that serves a test page with a cross-origin iframe for Find
// in Page. `sourceURL` is used as `src` for the iframe.
std::unique_ptr<net::test_server::HttpResponse>
FindInPageTestCrossOriginFramePageHttpResponse(
    const GURL& sourceURL,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kFindInPageCrossOriginFrameTestURL) {
    return nullptr;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(
      "<html><head><meta charset=\"UTF-8\"></head><body>" +
      FindInPageTestContent() + "<iframe src=\"" + sourceURL.spec() +
      "\"></iframe></body></html>");
  return std::move(http_response);
}

// Constants to identify the Find navigator UI components in the view hierarchy.
constexpr char kFindInPageDoneButtonID[] = "find.doneButton";
constexpr char kFindInPageSearchFieldID[] = "find.searchField";
constexpr char kFindInPageResultLabelID[] = "find.resultLabel";
constexpr char kFindInPageNextButtonID[] = "find.nextButton";
constexpr char kFindInPagePreviousButtonID[] = "find.previousButton";
constexpr char kFindInPageClearButtonKindOfClassName[] =
    "_UITextFieldClearButton";

// Returns Paste button matcher from UIMenuController.
id<GREYMatcher> PasteButton() {
  NSString* a11yLabelPaste = @"Paste";
  return grey_allOf(grey_accessibilityLabel(a11yLabelPaste),
                    chrome_test_util::SystemSelectionCallout(), nil);
}

// Long presses on `element_id` to trigger context menu.
void LongPressElement(const char* element_id) {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:element_id],
                        true /* menu should appear */)];
}

}  // namespace

@implementation FindInPageTestCaseHelper {
  // Second test server for cross-origin iframe tests.
  std::unique_ptr<net::test_server::EmbeddedTestServer> _secondTestServer;
}

- (void)setUpTestServersForWebPageTest {
  // Set up first server to test Find in Page content.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&FindInPageTestPageHttpResponse));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  // Set up second server for cross-origin iframe tests.
  GURL sourceURLForFrame = self.testServer->GetURL(kFindInPageTestURL);
  _secondTestServer = std::make_unique<net::test_server::EmbeddedTestServer>();
  [self secondTestServer]->RegisterRequestHandler(base::BindRepeating(
      &FindInPageTestCrossOriginFramePageHttpResponse, sourceURLForFrame));
  GREYAssertTrue([self secondTestServer]->Start(),
                 @"Second test server serving page with iframe did not start.");
}

- (net::test_server::EmbeddedTestServer*)secondTestServer {
  return _secondTestServer.get();
}

- (void)setUpTestServerForPDFTest {
  // This is sufficient to ensure `ios/testing/data/http_server_files/` file
  // system directory is being served, as this is the default configuration.
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

- (void)openFindInPageWithOverflowMenu {
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
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:[self findInPageInputField]];
  [ChromeEarlGreyUI waitForAppToIdle];
}

- (void)closeFindInPageWithDoneButton {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@(kFindInPageDoneButtonID))]
      performAction:grey_tap()];
}

- (void)typeFindInPageText:(NSString*)text {
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      performAction:grey_typeText(text)];
}

- (void)pasteTextToFindInPage:(NSString*)text {
  [ChromeEarlGrey copyTextToPasteboard:text];
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PasteButton()] performAction:grey_tap()];
}

- (void)clearFindInPageText {
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClassName(
                                   @(kFindInPageClearButtonKindOfClassName))]
      performAction:grey_tap()];
}

- (id<GREYMatcher>)findInPageInputField {
  return grey_accessibilityID(@(kFindInPageSearchFieldID));
}

- (id<GREYMatcher>)matcherForText:(NSString*)text {
  NSString* prefix = @"hasText";
  GREYMatchesBlock matchesBlock = ^BOOL(id element) {
    return [[element text] isEqualToString:text];
  };

  GREYDescribeToBlock describeToBlock = ^void(id<GREYDescription> description) {
    [description
        appendText:[NSString stringWithFormat:@"%@('%@')", prefix, text]];
  };
  // A matcher for non-SwiftUI elements
  id<GREYMatcher> matcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matchesBlock
                                           descriptionBlock:describeToBlock];
  return matcher;
}

- (void)assertResultStringIsResult:(int)resultIndex
                        outOfTotal:(int)resultCount {
  // Returns "<current> of <total>" search results label (e.g "1 of 5").
  NSString* expectedResultsString =
      [NSString stringWithFormat:@"%d of %d", resultIndex, resultCount];

  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            @(kFindInPageResultLabelID))]
        assertWithMatcher:chrome_test_util::StaticTextWithAccessibilityLabel(
                              expectedResultsString)
                    error:&error];
    return (error == nil);
  };
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(1), condition),
      @"Timeout waiting for correct Find in Page results string to appear");
}

- (void)assertResultStringIsEmptyOrZero {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            @(kFindInPageResultLabelID))]
        assertWithMatcher:
            grey_anyOf(chrome_test_util::StaticTextWithAccessibilityLabel(@"0"),
                       grey_notVisible(), nil)
                    error:&error];
    return (error == nil);
  };
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(1), condition),
      @"Timeout waiting for correct Find in Page results string to appear");
}

- (void)assertResultStringIsNonZero {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            @(kFindInPageResultLabelID))]
        assertWithMatcher:
            grey_not(grey_anyOf(
                chrome_test_util::StaticTextWithAccessibilityLabel(@"0"),
                grey_notVisible(), nil))
                    error:&error];
    return (error == nil);
  };
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(1), condition),
      @"Timeout waiting for correct Find in Page results string to appear");
}

- (void)advanceToNextResult {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@(kFindInPageNextButtonID))]
      performAction:grey_tap()];
}

- (void)advanceToPreviousResult {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @(kFindInPagePreviousButtonID))]
      performAction:grey_tap()];
}

// Tests that FIP can be opened with Overflow menu.
- (void)helperTestFindInPageFromOverflowMenu {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page.
    GURL destinationURL = self.testServer->GetURL(kFindInPageTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP with Overflow menu and check it is visible.
    [self openFindInPageWithOverflowMenu];
    [ChromeEarlGrey
        waitForSufficientlyVisibleElementWithMatcher:[self
                                                         findInPageInputField]];
  }
}

// Tests that characters appear in the search box and that results UI updates as
// each characters is entered/deleted.
- (void)helperTestFindInPageTextInput {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page.
    GURL destinationURL = self.testServer->GetURL(kFindInPageTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP.
    [self openFindInPageWithOverflowMenu];
    // Test the result string is empty or "0".
    [self assertResultStringIsEmptyOrZero];

    [self typeFindInPageText:@(kFindInPageTestRepeatingText)];
    // Test the input field contains the text that was just typed.
    [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
        assertWithMatcher:[self
                              matcherForText:@(kFindInPageTestRepeatingText)]];
    // Test the result UI is updated accordingly.
    [self assertResultStringIsResult:1 outOfTotal:2];

    [self typeFindInPageText:@(kFindInPageTestRepeatingText)];
    [self assertResultStringIsResult:1 outOfTotal:1];

    [self typeFindInPageText:@(kFindInPageTestRepeatingText)];
    [self assertResultStringIsEmptyOrZero];

    [self clearFindInPageText];
    [self assertResultStringIsEmptyOrZero];
  }
}

// Tests that the number of results for a query accounts for all the matches
// across frames, here with a main frame and a cross-origin iframe.
- (void)helperTestFindInPageSupportsCrossOriginFrame {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe.
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP.
    [self openFindInPageWithOverflowMenu];
    [self assertResultStringIsEmptyOrZero];

    [self typeFindInPageText:@(kFindInPageTestShortText)];
    // Tests there are two matches: one is in the main frame, the other in the
    // cross-origin iframe.
    [self assertResultStringIsResult:1 outOfTotal:2];

    [self advanceToNextResult];
    // Tests that the second match can be navigated to.
    [self assertResultStringIsResult:2 outOfTotal:2];
  }
}

// Tests that FIP can find different types of characters: special characters,
// number, strings with both letters and numbers as well as non-ASCII
// characters.
- (void)helperTestFindInPageSpecialCharacters {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe.
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP.
    [self openFindInPageWithOverflowMenu];
    [self assertResultStringIsEmptyOrZero];

    // Tests special characters.
    [self typeFindInPageText:@(kFindInPageTestSpecialCharactersText)];
    [self assertResultStringIsResult:1 outOfTotal:2];

    // Tests numbers.
    [self clearFindInPageText];
    [self typeFindInPageText:@(kFindInPageTestNumbersText)];
    [self assertResultStringIsResult:1 outOfTotal:2];

    // Tests alphanumeric values.
    [self clearFindInPageText];
    [self typeFindInPageText:@(kFindInPageTestAlphanumericText)];
    [self assertResultStringIsResult:1 outOfTotal:2];

    // Tests non-ASCII characters.
    [self clearFindInPageText];
    [self pasteTextToFindInPage:@(kFindInPageTestNonASCIIText)];
    [self assertResultStringIsResult:1 outOfTotal:2];
  }
}

// Tests that text can be copied from the web page and pasted into the FIP input
// field and that the results UI updates accordingly.
- (void)helperTestFindInPageCopyPaste {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page.
    GURL destinationURL = self.testServer->GetURL(kFindInPageTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Select and copy text on the web page.
    LongPressElement(kFindInPageTestShortTextID);
    [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                            SystemSelectionCalloutCopyButton()]
        performAction:grey_tap()];

    // Open FIP.
    [self openFindInPageWithOverflowMenu];

    // Paste content of pasteboard in the FIP text field.
    [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:PasteButton()]
        performAction:grey_tap()];

    // Tests that the number of results is updated accordingly.
    [self assertResultStringIsResult:1 outOfTotal:1];
  }
}

// Tests that FIP yields no results for an empty search query.
- (void)helperTestFindInPageEmptySearchQuery {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe.
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP.
    [self openFindInPageWithOverflowMenu];

    // Assert that searching text from the page yields results.
    [self typeFindInPageText:@(kFindInPageTestShortText)];
    [self assertResultStringIsNonZero];

    // Test that the number of results is zero after clearing the FIP text
    // field.
    [self clearFindInPageText];
    [self assertResultStringIsEmptyOrZero];
  }
}

// Tests that FIP yields no results for a non-empty query with no matches in the
// page.
- (void)helperTestFindInPageQueryWithNoMatches {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe.
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP and type text that is not in the page.
    const char* queryWithNoMatches =
        "example query which should not match with the content of the page";
    [ChromeEarlGrey waitForWebStateNotContainingText:queryWithNoMatches];
    [self openFindInPageWithOverflowMenu];
    [self typeFindInPageText:@(queryWithNoMatches)];
    // Test the result label shows no results.
    [self assertResultStringIsEmptyOrZero];
  }
}

// Tests that FIP yields no matches for a text with spanish accents e.g. 'á' if
// the web page contains the same text without spanish accents e.g. 'a'. This
// test assumes removing accents from `kFindInPageTestWithSpanishAccentText`
// yields `kFindInPageTestWithoutSpanishAccentText`.
- (void)helperTestFindInPageDifferentAccent {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe.
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Assert the text without accent is there but the text with accents does
    // not match.
    [ChromeEarlGrey
        waitForWebStateContainingText:kFindInPageTestWithoutSpanishAccentText];
    [ChromeEarlGrey
        waitForWebStateNotContainingText:kFindInPageTestWithSpanishAccentText];

    // Open FIP and assert that text with no accents yields matches.
    [self openFindInPageWithOverflowMenu];
    [self typeFindInPageText:@(kFindInPageTestWithoutSpanishAccentText)];
    [self assertResultStringIsNonZero];

    // Replace the text without spanish accent with the same text with spanish
    // accents and test that there are no more matches.
    [self clearFindInPageText];
    [self pasteTextToFindInPage:@(kFindInPageTestWithSpanishAccentText)];
    [self assertResultStringIsEmptyOrZero];
  }
}

// Test that there is no query persistence with this variant of Native Find in
// Page i.e. with Find interaction.
- (void)helperTestFindInPageHistory {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe.
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP and test the input field is empty.
    [self openFindInPageWithOverflowMenu];
    [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
        assertWithMatcher:[self matcherForText:@""]];

    // Type a query and assert it is contained in the input field before closing
    // FIP.
    [self typeFindInPageText:@(kFindInPageTestShortText)];
    [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
        assertWithMatcher:[self matcherForText:@(kFindInPageTestShortText)]];
    [self closeFindInPageWithDoneButton];

    // Open FIP again and test it is empty again (no query persistence).
    [self openFindInPageWithOverflowMenu];
    [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
        assertWithMatcher:[self matcherForText:@""]];

    // Open the same URL in a different non-Incognito tab.
    [ChromeEarlGrey openNewTab];
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP in this new tab and test there is no query persistence.
    [self openFindInPageWithOverflowMenu];
    [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
        assertWithMatcher:[self matcherForText:@""]];
  }
}

// Tests that there is no query persistence from an non-Incognito to an
// Incognito tab.
- (void)helperTestFindInPageNormalToIncognito {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe.
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP and type short query.
    [self openFindInPageWithOverflowMenu];
    [self typeFindInPageText:@(kFindInPageTestShortText)];

    // Load same URL in a new Incognito tab.
    [ChromeEarlGrey openNewIncognitoTab];
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP and test the input field is empty.
    [self openFindInPageWithOverflowMenu];
    [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
        assertWithMatcher:[self matcherForText:@""]];
  }
}

// Tests that switching orientation during a Find session does not throw away
// the query or the current results.
- (void)helperTestFindInPageSwitchOrientation {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe.
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP, type short query, move to second match and wait for expected
    // results.
    [self openFindInPageWithOverflowMenu];
    [self typeFindInPageText:@(kFindInPageTestShortText)];
    [self advanceToNextResult];
    [self assertResultStringIsResult:2 outOfTotal:2];

    // Switch to landscape.
    GREYAssert(
        [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                      error:nil],
        @"Could not rotate device to Landscape Left");

    // Test the query is still there will the same result.
    [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
        assertWithMatcher:[self matcherForText:@(kFindInPageTestShortText)]];
    [self assertResultStringIsResult:2 outOfTotal:2];

    // Switch back to portrait.
    GREYAssert([EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                                             error:nil],
               @"Could not rotate device to Portrait");

    // Test the query is still there will the same result.
    [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
        assertWithMatcher:[self matcherForText:@(kFindInPageTestShortText)]];
    [self assertResultStringIsResult:2 outOfTotal:2];
  }
}

// Tests that Next/Previous buttons work and wrap.
- (void)helperTestFindInPageNextPreviousArrows {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe.
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP and type query with four expected matches.
    [self openFindInPageWithOverflowMenu];
    [self typeFindInPageText:@(kFindInPageTestRepeatingText)];
    [self assertResultStringIsResult:1 outOfTotal:4];

    // Test that tapping "Next" button works and wraps.
    [self advanceToNextResult];
    [self assertResultStringIsResult:2 outOfTotal:4];
    [self advanceToNextResult];
    [self assertResultStringIsResult:3 outOfTotal:4];
    [self advanceToNextResult];
    [self assertResultStringIsResult:4 outOfTotal:4];
    [self advanceToNextResult];
    [self assertResultStringIsResult:1 outOfTotal:4];

    // Test that tapping "Previous" button also works and wraps.
    [self advanceToPreviousResult];
    [self assertResultStringIsResult:4 outOfTotal:4];
    [self advanceToPreviousResult];
    [self assertResultStringIsResult:3 outOfTotal:4];
  }
}

// Tests the various ways to dismiss the keyboard during a Find session.
- (void)helperTestFindInPageDismissKeyboard {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe.
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP and type short query.
    [self openFindInPageWithOverflowMenu];
    [self typeFindInPageText:@(kFindInPageTestShortText)];

    // Tap Done button and test the keyboard is dismissed as a result.
    [self closeFindInPageWithDoneButton];
    GREYAssertFalse([EarlGrey isKeyboardShownWithError:nil],
                    @"Keyboard Should be Hidden");

    // Open FIP and type short query again.
    [self openFindInPageWithOverflowMenu];
    [self typeFindInPageText:@(kFindInPageTestShortText)];

    // Tap an element on the page and test the keyboard is dismissed as a
    // result.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
        performAction:chrome_test_util::TapWebElementWithId(
                          kFindInPageTestShortTextID)];
    GREYAssertFalse([EarlGrey isKeyboardShownWithError:nil],
                    @"Keyboard Should be Hidden");
  }
}

// Tests that FIP can find long strings of characters.
- (void)helperTestFindInPageLongString {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe.
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP and type short query.
    [self openFindInPageWithOverflowMenu];
    [self typeFindInPageText:@(kFindInPageTestLongText)];

    // Test the number of results is as expected.
    [self assertResultStringIsResult:1 outOfTotal:2];
  }
}

// Tests that FIP is not case sensitive.
- (void)helperTestFindInPageNotCaseSensitive {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe.
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Assert the page contains string that is both lowercase and uppercase.
    [ChromeEarlGrey
        waitForWebStateContainingText:kFindInPageTestLowercaseAndUppercaseText];

    // Open FIP and type lowercase version of contained text.
    [self openFindInPageWithOverflowMenu];
    [self typeFindInPageText:[@(kFindInPageTestLowercaseAndUppercaseText)
                                 lowercaseString]];
    // Test the number of results is as expected.
    [self assertResultStringIsResult:1 outOfTotal:2];

    // Clear input field and type uppercase version of contained text.
    [self clearFindInPageText];
    [self typeFindInPageText:[@(kFindInPageTestLowercaseAndUppercaseText)
                                 uppercaseString]];
    // Test the number of results is as expected.
    [self assertResultStringIsResult:1 outOfTotal:2];
  }
}

// Tests that there is no leak of the FIP search query from Incognito tabs to
// normal tabs.
- (void)helperTestFindInPageIncognitoHistory {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe in new Incognito tab.
    [ChromeEarlGrey openNewIncognitoTab];
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP and type short query.
    [self openFindInPageWithOverflowMenu];
    [self typeFindInPageText:@(kFindInPageTestShortText)];

    // Open a new normal tab and load the same URL.
    [ChromeEarlGrey openNewTab];
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP again and test the input field is empty.
    [self openFindInPageWithOverflowMenu];
    [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
        assertWithMatcher:[self matcherForText:@""]];
  }
}

// Tests that there is no query persistence when coming back to a normal tab
// after switching temporarily to another tab.
- (void)helperTestFindInPageSwitchingTabs {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe in a second normal tab.
    [ChromeEarlGrey openNewTab];
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP and type short query.
    [self openFindInPageWithOverflowMenu];
    [self typeFindInPageText:@(kFindInPageTestShortText)];

    // Switching to first tab and then back to second tab.
    [ChromeEarlGrey selectTabAtIndex:0];
    [ChromeEarlGrey selectTabAtIndex:1];

    // Test that there is no query persistence (input field is empty).
    [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
        assertWithMatcher:[self matcherForText:@""]];
  }
}

// Tests that FIP can find RTL text in a web page.
- (void)helperTestFindInPageRTL {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe.
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP, type RTL text and test that the results are as expected.
    [self openFindInPageWithOverflowMenu];
    [self pasteTextToFindInPage:@(kFindInPageTestRTLText)];
    [self assertResultStringIsResult:1 outOfTotal:2];
  }
}

// Tests that Find in Page can find matches in an Incognito tab.
- (void)helperTestFindInPageIncognito {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe in a new Incognito tab.
    [ChromeEarlGrey openNewIncognitoTab];
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP, type text contained in test page and test that the results are
    // as expected.
    [self openFindInPageWithOverflowMenu];
    [self typeFindInPageText:@(kFindInPageTestShortText)];
    [self assertResultStringIsResult:1 outOfTotal:2];
  }
}

// Tests accessibility of the Find in Page screen.
- (void)helperTestFindInPageAccessibility {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe.
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP, type query and check the expected number of results.
    [self openFindInPageWithOverflowMenu];
    [self typeFindInPageText:@(kFindInPageTestShortText)];
    [self assertResultStringIsResult:1 outOfTotal:2];

    // Test accessibility.
    [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  }
}

// Tests that Native Find in Page works as expected for PDF documents.
- (void)helperTestFindInPagePDF {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServerForPDFTest];

    // Load test PDF document.
    GURL destinationURL = self.testServer->GetURL(kFindInPageComplexPDFTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP and test that the input field is empty and there are no results.
    [self openFindInPageWithOverflowMenu];
    [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
        assertWithMatcher:[self matcherForText:@""]];
    [self assertResultStringIsEmptyOrZero];

    // Type text with 157 expected matches and test that results are as
    // expected.
    [self typeFindInPageText:@"the"];
    [self assertResultStringIsResult:1 outOfTotal:157];

    // Test that the Next button works.
    [self advanceToNextResult];
    [self assertResultStringIsResult:2 outOfTotal:157];
    [self advanceToNextResult];
    [self assertResultStringIsResult:3 outOfTotal:157];

    // Type more specific query and test that results are as expected.
    [self typeFindInPageText:@" Form"];
    [self assertResultStringIsResult:1 outOfTotal:6];

    // Test that the Previous button works and wraps.
    [self advanceToPreviousResult];
    [self assertResultStringIsResult:6 outOfTotal:6];
    [self advanceToPreviousResult];
    [self assertResultStringIsResult:5 outOfTotal:6];

    // Type even more specific query and test that results are as expected.
    [self typeFindInPageText:@" 1050"];
    [self assertResultStringIsEmptyOrZero];

    // Test that the Done button does close Find in Page.
    [self closeFindInPageWithDoneButton];
    [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
        assertWithMatcher:grey_notVisible()];
  }
}

@end

// Tests for Native Find in Page. This tests the variant of Native Find in Page
// with a Find interaction i.e. with the system UI or Find navigator. Many tests
// use the `secondTestServer` to ensure what is being tested also works with
// cross-origin iframes.
@interface FindInPageTestCase : ChromeTestCase

@end

@implementation FindInPageTestCase {
  FindInPageTestCaseHelper* _helper;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  // Enable `kNativeFindInPage`. This test case only tests the "System Find
  // Panel" variant of Native Find in Page i.e. the one relying on a Find
  // interaction and the new system UI.
  config.additional_args.push_back(
      "--enable-features=" + std::string(kNativeFindInPage.name) + "<" +
      std::string(kNativeFindInPage.name));
  config.additional_args.push_back(
      "--force-fieldtrials=" + std::string(kNativeFindInPage.name) + "/Test");

  return config;
}

- (void)setUp {
  [super setUp];

  // Clear saved search term.
  [FindInPageAppInterface clearSearchTerm];

  // Creating helper.
  _helper = [[FindInPageTestCaseHelper alloc] init];
  _helper.testServer = self.testServer;
}

// Tests that FIP can be opened with Overflow menu.
- (void)helperTestFindInPageFromOverflowMenu {
  [_helper helperTestFindInPageFromOverflowMenu];
}

// Tests that characters appear in the search box and that results UI updates as
// each characters is entered/deleted.
- (void)testFindInPageTextInput {
  [_helper helperTestFindInPageTextInput];
}

// Tests that the number of results for a query accounts for all the matches
// across frames, here with a main frame and a cross-origin iframe.
- (void)testFindInPageSupportsCrossOriginFrame {
  [_helper helperTestFindInPageSupportsCrossOriginFrame];
}

// Tests that FIP can find different types of characters: special characters,
// number, strings with both letters and numbers as well as non-ASCII
// characters.
- (void)testFindInPageSpecialCharacters {
  [_helper helperTestFindInPageSpecialCharacters];
}

// Tests that text can be copied from the web page and pasted into the FIP input
// field and that the results UI updates accordingly.
- (void)testFindInPageCopyPaste {
  [_helper helperTestFindInPageCopyPaste];
}

// Tests that FIP yields no results for an empty search query.
- (void)testFindInPageEmptySearchQuery {
  [_helper helperTestFindInPageEmptySearchQuery];
}

// Tests that FIP yields no results for a non-empty query with no matches in the
// page.
- (void)testFindInPageQueryWithNoMatches {
  [_helper helperTestFindInPageQueryWithNoMatches];
}

// Tests that FIP yields no matches for a text with spanish accents e.g. 'á' if
// the web page contains the same text without spanish accents e.g. 'a'. This
// test assumes removing accents from `kFindInPageTestWithSpanishAccentText`
// yields `kFindInPageTestWithoutSpanishAccentText`.
- (void)testFindInPageDifferentAccent {
  [_helper helperTestFindInPageDifferentAccent];
}

// Test that there is no query persistence with this variant of Native Find in
// Page i.e. with Find interaction.
- (void)testFindInPageHistory {
  [_helper helperTestFindInPageHistory];
}

// Tests that there is no query persistence from an non-Incognito to an
// Incognito tab.
- (void)testFindInPageNormalToIncognito {
  [_helper helperTestFindInPageNormalToIncognito];
}

// Tests that Next/Previous buttons work and wrap.
- (void)testFindInPageNextPreviousArrows {
  [_helper helperTestFindInPageNextPreviousArrows];
}

// Tests the various ways to dismiss the keyboard during a Find session.
- (void)testFindInPageDismissKeyboard {
  [_helper helperTestFindInPageDismissKeyboard];
}

// Tests that FIP can find long strings of characters.
- (void)testFindInPageLongString {
  [_helper helperTestFindInPageLongString];
}

// Tests that FIP is not case sensitive.
- (void)testFindInPageNotCaseSensitive {
  [_helper helperTestFindInPageNotCaseSensitive];
}

// Tests that there is no leak of the FIP search query from Incognito tabs to
// normal tabs.
- (void)testFindInPageIncognitoHistory {
  [_helper helperTestFindInPageIncognitoHistory];
}

// Tests that there is no query persistence when coming back to a normal tab
// after switching temporarily to another tab.
- (void)testFindInPageSwitchingTabs {
  [_helper helperTestFindInPageSwitchingTabs];
}

// Tests that FIP can find RTL text in a web page.
- (void)testFindInPageRTL {
  [_helper helperTestFindInPageRTL];
}

// Tests that Find in Page can find matches in an Incognito tab.
- (void)testFindInPageIncognito {
  [_helper helperTestFindInPageIncognito];
}

// Tests accessibility of the Find in Page screen.
- (void)testFindInPageAccessibility {
  [_helper helperTestFindInPageAccessibility];
}

// Tests that Native Find in Page works as expected for PDF documents.
- (void)testFindInPagePDF {
  [_helper helperTestFindInPagePDF];
}

@end
