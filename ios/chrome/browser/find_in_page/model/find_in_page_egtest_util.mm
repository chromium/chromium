// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/model/find_in_page_egtest_util.h"

#import <sstream>

#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

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

// Long presses on `element_id` to trigger context menu.
void LongPressElement(const char* element_id) {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:element_id],
                        true /* menu should appear */)];
}

}  // namespace

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

id<GREYMatcher> PasteButton() {
  NSString* a11yLabelPaste = @"Paste";
  return grey_allOf(grey_accessibilityLabel(a11yLabelPaste),
                    chrome_test_util::SystemSelectionCallout(), nil);
}

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

// Tests that FIP can be opened with Overflow menu.
- (void)helperTestFindInPageFromOverflowMenu {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page.
    GURL destinationURL = self.testServer->GetURL(kFindInPageTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP with Overflow menu and check it is visible.
    [self.delegate openFindInPageWithOverflowMenu];
    [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                        [self.delegate findInPageInputField]];
    [self.delegate closeFindInPageWithDoneButton];
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
    [self.delegate openFindInPageWithOverflowMenu];
    // Test the result string is empty or "0".
    [self.delegate assertResultStringIsEmptyOrZero];

    [self.delegate replaceFindInPageText:@(kFindInPageTestRepeatingText)];
    // Test the input field contains the text that was just typed.
    [[EarlGrey selectElementWithMatcher:[self.delegate findInPageInputField]]
        assertWithMatcher:[self
                              matcherForText:@(kFindInPageTestRepeatingText)]];
    // Test the result UI is updated accordingly.
    [self.delegate assertResultStringIsResult:1 outOfTotal:2];

    [self.delegate
        replaceFindInPageText:
            [NSString stringWithFormat:@"%s%s", kFindInPageTestRepeatingText,
                                       kFindInPageTestRepeatingText]];
    [self.delegate assertResultStringIsResult:1 outOfTotal:1];

    [self.delegate
        replaceFindInPageText:
            [NSString stringWithFormat:@"%s%s%s", kFindInPageTestRepeatingText,
                                       kFindInPageTestRepeatingText,
                                       kFindInPageTestRepeatingText]];
    [self.delegate assertResultStringIsEmptyOrZero];

    [self.delegate clearFindInPageText];
    [self.delegate assertResultStringIsEmptyOrZero];
    [self.delegate closeFindInPageWithDoneButton];
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
    [self.delegate openFindInPageWithOverflowMenu];
    [self.delegate assertResultStringIsEmptyOrZero];

    [self.delegate replaceFindInPageText:@(kFindInPageTestShortText)];
    // Tests there are two matches: one is in the main frame, the other in the
    // cross-origin iframe.
    [self.delegate assertResultStringIsResult:1 outOfTotal:2];

    [self.delegate advanceToNextResult];
    // Tests that the second match can be navigated to.
    [self.delegate assertResultStringIsResult:2 outOfTotal:2];
    [self.delegate closeFindInPageWithDoneButton];
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
    [self.delegate openFindInPageWithOverflowMenu];
    [self.delegate assertResultStringIsEmptyOrZero];

    // Tests special characters.
    [self.delegate
        replaceFindInPageText:@(kFindInPageTestSpecialCharactersText)];
    [self.delegate assertResultStringIsResult:1 outOfTotal:2];

    // Tests numbers.
    [self.delegate replaceFindInPageText:@(kFindInPageTestNumbersText)];
    [self.delegate assertResultStringIsResult:1 outOfTotal:2];

    // Tests alphanumeric values.
    [self.delegate replaceFindInPageText:@(kFindInPageTestAlphanumericText)];
    [self.delegate assertResultStringIsResult:1 outOfTotal:2];

    // Tests non-ASCII characters.
    [self.delegate replaceFindInPageText:@(kFindInPageTestNonASCIIText)];
    [self.delegate assertResultStringIsResult:1 outOfTotal:2];
    [self.delegate closeFindInPageWithDoneButton];
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

    [[EarlGrey
        selectElementWithMatcher:
            grey_allOf(chrome_test_util::SystemSelectionCalloutCopyButton(),
                       grey_sufficientlyVisible(), nil)]
        performAction:grey_tap()];

    // Open FIP.
    [self.delegate openFindInPageWithOverflowMenu];

    // Paste content of pasteboard in the FIP text field.
    [[EarlGrey selectElementWithMatcher:[self.delegate findInPageInputField]]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:PasteButton()]
        performAction:grey_tap()];

    // Tests that the number of results is updated accordingly.
    [self.delegate assertResultStringIsResult:1 outOfTotal:1];
    [self.delegate closeFindInPageWithDoneButton];
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
    [self.delegate openFindInPageWithOverflowMenu];

    // Assert that searching text from the page yields results.
    [self.delegate replaceFindInPageText:@(kFindInPageTestShortText)];
    [self.delegate assertResultStringIsNonZero];

    // Test that the number of results is zero after clearing the FIP text
    // field.
    [self.delegate clearFindInPageText];
    [self.delegate assertResultStringIsEmptyOrZero];
    [self.delegate closeFindInPageWithDoneButton];
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
    [self.delegate openFindInPageWithOverflowMenu];
    [self.delegate replaceFindInPageText:@(queryWithNoMatches)];
    // Test the result label shows no results.
    [self.delegate assertResultStringIsEmptyOrZero];
    [self.delegate closeFindInPageWithDoneButton];
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
    [self.delegate openFindInPageWithOverflowMenu];
    [self.delegate
        replaceFindInPageText:@(kFindInPageTestWithoutSpanishAccentText)];
    [self.delegate assertResultStringIsNonZero];

    // Replace the text without spanish accent with the same text with spanish
    // accents and test that there are no more matches.
    [self.delegate
        replaceFindInPageText:@(kFindInPageTestWithSpanishAccentText)];
    [self.delegate assertResultStringIsEmptyOrZero];
    [self.delegate closeFindInPageWithDoneButton];
  }
}

// Test query persistence in the same tab and in a new normal tab.
- (void)helperTestFindInPageHistoryWithQueryPersistence:(BOOL)queryPersistence {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe.
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP and test the input field is empty.
    [self.delegate openFindInPageWithOverflowMenu];
    [[EarlGrey selectElementWithMatcher:[self.delegate findInPageInputField]]
        assertWithMatcher:[self matcherForText:@""]];

    // Type a query and assert it is contained in the input field before closing
    // FIP.
    [self.delegate replaceFindInPageText:@(kFindInPageTestShortText)];
    [[EarlGrey selectElementWithMatcher:[self.delegate findInPageInputField]]
        assertWithMatcher:[self matcherForText:@(kFindInPageTestShortText)]];
    [self.delegate closeFindInPageWithDoneButton];

    // Open FIP again and test depending on query persistence.
    [self.delegate openFindInPageWithOverflowMenu];
    [[EarlGrey selectElementWithMatcher:[self.delegate findInPageInputField]]
        assertWithMatcher:[self matcherForText:queryPersistence
                                                   ? @(kFindInPageTestShortText)
                                                   : @""]];
    [self.delegate closeFindInPageWithDoneButton];

    // Open the same URL in a different non-Incognito tab.
    [ChromeEarlGrey openNewTab];
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP in this new tab and test depending on query persistence.
    [self.delegate openFindInPageWithOverflowMenu];
    [[EarlGrey selectElementWithMatcher:[self.delegate findInPageInputField]]
        assertWithMatcher:[self matcherForText:queryPersistence
                                                   ? @(kFindInPageTestShortText)
                                                   : @""]];
    [self.delegate closeFindInPageWithDoneButton];
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
    [self.delegate openFindInPageWithOverflowMenu];
    [self.delegate replaceFindInPageText:@(kFindInPageTestShortText)];
    [self.delegate closeFindInPageWithDoneButton];

    // Load same URL in a new Incognito tab.
    [ChromeEarlGrey openNewIncognitoTab];
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP and test the input field is empty.
    [self.delegate openFindInPageWithOverflowMenu];
    [[EarlGrey selectElementWithMatcher:[self.delegate findInPageInputField]]
        assertWithMatcher:[self matcherForText:@""]];
    [self.delegate closeFindInPageWithDoneButton];
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
    [self.delegate openFindInPageWithOverflowMenu];
    [self.delegate replaceFindInPageText:@(kFindInPageTestShortText)];
    [self.delegate advanceToNextResult];
    [self.delegate assertResultStringIsResult:2 outOfTotal:2];

    // Switch to landscape.
    GREYAssert(
        [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                      error:nil],
        @"Could not rotate device to Landscape Left");

    // Test the query is still there will the same result.
    [[EarlGrey selectElementWithMatcher:[self.delegate findInPageInputField]]
        assertWithMatcher:[self matcherForText:@(kFindInPageTestShortText)]];
    [self.delegate assertResultStringIsResult:2 outOfTotal:2];

    // Switch back to portrait.
    GREYAssert([EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                                             error:nil],
               @"Could not rotate device to Portrait");

    // Test the query is still there will the same result.
    [[EarlGrey selectElementWithMatcher:[self.delegate findInPageInputField]]
        assertWithMatcher:[self matcherForText:@(kFindInPageTestShortText)]];
    [self.delegate assertResultStringIsResult:2 outOfTotal:2];
    [self.delegate closeFindInPageWithDoneButton];
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
    [self.delegate openFindInPageWithOverflowMenu];
    [self.delegate replaceFindInPageText:@(kFindInPageTestRepeatingText)];
    [self.delegate assertResultStringIsResult:1 outOfTotal:4];

    // Test that tapping "Next" button works and wraps.
    [self.delegate advanceToNextResult];
    [self.delegate assertResultStringIsResult:2 outOfTotal:4];
    [self.delegate advanceToNextResult];
    [self.delegate assertResultStringIsResult:3 outOfTotal:4];
    [self.delegate advanceToNextResult];
    [self.delegate assertResultStringIsResult:4 outOfTotal:4];
    [self.delegate advanceToNextResult];
    [self.delegate assertResultStringIsResult:1 outOfTotal:4];

    // Test that tapping "Previous" button also works and wraps.
    [self.delegate advanceToPreviousResult];
    [self.delegate assertResultStringIsResult:4 outOfTotal:4];
    [self.delegate advanceToPreviousResult];
    [self.delegate assertResultStringIsResult:3 outOfTotal:4];
    [self.delegate closeFindInPageWithDoneButton];
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
    [self.delegate openFindInPageWithOverflowMenu];
    [self.delegate replaceFindInPageText:@(kFindInPageTestShortText)];

    // Tap Done button and test the keyboard is dismissed as a result.
    [self.delegate closeFindInPageWithDoneButton];
    [ChromeEarlGrey waitForKeyboardToDisappear];

    // Open FIP and type short query again.
    [self.delegate openFindInPageWithOverflowMenu];
    [self.delegate replaceFindInPageText:@(kFindInPageTestShortText)];

    // Tap an element on the page and test the keyboard is dismissed as a
    // result.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
        performAction:chrome_test_util::TapWebElementWithId(
                          kFindInPageTestShortTextID)];
    [ChromeEarlGrey waitForKeyboardToDisappear];
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
    [self.delegate openFindInPageWithOverflowMenu];
    [self.delegate replaceFindInPageText:@(kFindInPageTestLongText)];

    // Test the number of results is as expected.
    [self.delegate assertResultStringIsResult:1 outOfTotal:2];
    [self.delegate closeFindInPageWithDoneButton];
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
    [self.delegate openFindInPageWithOverflowMenu];
    [self.delegate
        replaceFindInPageText:[@(kFindInPageTestLowercaseAndUppercaseText)
                                  lowercaseString]];
    // Test the number of results is as expected.
    [self.delegate assertResultStringIsResult:1 outOfTotal:2];

    // Clear input field and type uppercase version of contained text.
    [self.delegate
        replaceFindInPageText:[@(kFindInPageTestLowercaseAndUppercaseText)
                                  uppercaseString]];
    // Test the number of results is as expected.
    [self.delegate assertResultStringIsResult:1 outOfTotal:2];
    [self.delegate closeFindInPageWithDoneButton];
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
    [self.delegate openFindInPageWithOverflowMenu];
    [self.delegate replaceFindInPageText:@(kFindInPageTestShortText)];
    [self.delegate closeFindInPageWithDoneButton];

    // Open a new normal tab and load the same URL.
    [ChromeEarlGrey openNewTab];
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP again and test the input field is empty.
    [self.delegate openFindInPageWithOverflowMenu];
    [[EarlGrey selectElementWithMatcher:[self.delegate findInPageInputField]]
        assertWithMatcher:[self matcherForText:@""]];
    [self.delegate closeFindInPageWithDoneButton];
  }
}

// Tests query persistence when coming back to a normal tab after switching
// temporarily to another tab.
- (void)helperTestFindInPageSwitchingTabsWithQueryPersistence:
    (BOOL)queryPersistence {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page with cross-origin iframe in a second normal tab.
    [ChromeEarlGrey openNewTab];
    GURL destinationURL =
        [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Open FIP and type short query.
    [self.delegate openFindInPageWithOverflowMenu];
    [self.delegate replaceFindInPageText:@(kFindInPageTestShortText)];

    // Switching to first tab and then back to second tab.
    [ChromeEarlGrey selectTabAtIndex:0];
    [ChromeEarlGrey selectTabAtIndex:1];

    // Test query persistence.
    [[EarlGrey selectElementWithMatcher:[self.delegate findInPageInputField]]
        assertWithMatcher:[self matcherForText:queryPersistence
                                                   ? @(kFindInPageTestShortText)
                                                   : @""]];
    [self.delegate closeFindInPageWithDoneButton];
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
    [self.delegate openFindInPageWithOverflowMenu];
    [self.delegate pasteTextToFindInPage:@(kFindInPageTestRTLText)];
    [self.delegate assertResultStringIsResult:1 outOfTotal:2];
    [self.delegate closeFindInPageWithDoneButton];
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
    [self.delegate openFindInPageWithOverflowMenu];
    [self.delegate replaceFindInPageText:@(kFindInPageTestShortText)];
    [self.delegate assertResultStringIsResult:1 outOfTotal:2];
    [self.delegate closeFindInPageWithDoneButton];
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
    [self.delegate openFindInPageWithOverflowMenu];
    [self.delegate replaceFindInPageText:@(kFindInPageTestShortText)];
    [self.delegate assertResultStringIsResult:1 outOfTotal:2];

    // Test accessibility.
    [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
    [self.delegate closeFindInPageWithDoneButton];
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
    [self.delegate openFindInPageWithOverflowMenu];
    [[EarlGrey selectElementWithMatcher:[self.delegate findInPageInputField]]
        assertWithMatcher:[self matcherForText:@""]];
    [self.delegate assertResultStringIsEmptyOrZero];

    // Type text with 18 expected matches and test that results are as
    // expected.
    [self.delegate replaceFindInPageText:@"the F"];
    [self.delegate assertResultStringIsResult:1 outOfTotal:18];

    // Test that the Next button works.
    [self.delegate advanceToNextResult];
    [self.delegate assertResultStringIsResult:2 outOfTotal:18];
    [self.delegate advanceToNextResult];
    [self.delegate assertResultStringIsResult:3 outOfTotal:18];

    // Type more specific query and test that results are as expected.
    [self.delegate replaceFindInPageText:@"the Form"];
    [self.delegate assertResultStringIsResult:1 outOfTotal:6];

    // Test that the Previous button works and wraps.
    [self.delegate advanceToPreviousResult];
    [self.delegate assertResultStringIsResult:6 outOfTotal:6];
    [self.delegate advanceToPreviousResult];
    [self.delegate assertResultStringIsResult:5 outOfTotal:6];

    // Type even more specific query and test that results are as expected.
    [self.delegate replaceFindInPageText:@"the Form 1050"];
    [self.delegate assertResultStringIsEmptyOrZero];

    // Test that the Done button does close Find in Page.
    [self.delegate closeFindInPageWithDoneButton];
    [[EarlGrey selectElementWithMatcher:[self.delegate findInPageInputField]]
        assertWithMatcher:grey_notVisible()];
  }
}

// Tests that FIP exit fullscreen when done.
- (void)helperTestFindInPageExitFullscreen {
  if (@available(iOS 16.1.1, *)) {
    [self setUpTestServersForWebPageTest];

    // Load test page.
    GURL destinationURL = self.testServer->GetURL(kFindInPageComplexPDFTestURL);
    [ChromeEarlGrey loadURL:destinationURL];

    // Ensure the toolbars are not in fullscreen mode by checking if share
    // button is visible.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
        assertWithMatcher:grey_sufficientlyVisible()];

    // Open FIP with Overflow menu and check it is visible and the share button
    // is not visible.
    [self.delegate openFindInPageWithOverflowMenu];
    [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                        [self.delegate findInPageInputField]];

    [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                        chrome_test_util::TabShareButton()];

    // Close find in page with Done button and ensure the share button is
    // visible again.
    [self.delegate closeFindInPageWithDoneButton];
    [[EarlGrey selectElementWithMatcher:[self.delegate findInPageInputField]]
        assertWithMatcher:grey_notVisible()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}

@end
