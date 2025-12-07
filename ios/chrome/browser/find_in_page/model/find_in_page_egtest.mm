// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/find_in_page/model/find_in_page_app_interface.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/popup_menu_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

using chrome_test_util::WebStateScrollViewMatcher;

namespace {

// Constants for Find in Page test content.
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

// Relative URLs for testing purposes.
const char kFindInPageTestURL[] = "/findinpage.html";
const char kFindInPageCrossOriginFrameTestURL[] = "/crossorigin.html";
const char kFindInPageComplexPDFTestURL[] = "/complex_document.pdf";

// Constants to identify the Find navigator UI components in the view hierarchy.
constexpr char kFindInPageDoneButtonID[] = "find.doneButton";
constexpr char kFindInPageSearchFieldID[] = "find.searchField";
constexpr char kFindInPageResultLabelID[] = "find.resultLabel";
constexpr char kFindInPageNextButtonID[] = "find.nextButton";
constexpr char kFindInPagePreviousButtonID[] = "find.previousButton";

// Returns Paste button matcher from UIMenuController.
id<GREYMatcher> PasteButton() {
  NSString* a11yLabelPaste = @"Paste";
  return grey_allOf(grey_accessibilityLabel(a11yLabelPaste),
                    chrome_test_util::SystemSelectionCallout(), nil);
}

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
  oss << "<div style=\"height: 2000px; background-color: lightgray;\"/>";
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

}  // namespace

// Tests for Find in Page. Many tests use the `secondTestServer` to ensure what
// is being tested also works with cross-origin iframes.
@interface FindInPageTestCase : ChromeTestCase

@end

@implementation FindInPageTestCase {
  // Second test server for cross-origin iframe tests.
  std::unique_ptr<net::test_server::EmbeddedTestServer> _secondTestServer;
}

- (void)setUp {
  [super setUp];

  // Clear saved search term.
  [FindInPageAppInterface clearSearchTerm];
}

#pragma mark - Helpers

// Opens Find in Page.
- (void)openFindInPageWithOverflowMenu {
  [ChromeEarlGrey waitForKeyboardToDisappear];
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

// Closes Find in page.
- (void)closeFindInPageWithDoneButton {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@(kFindInPageDoneButtonID))]
      performAction:grey_tap()];
}

// Replaces the text in the Find in page textfield.
- (void)replaceFindInPageText:(NSString*)text {
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      performAction:grey_replaceText(text)];
}

// Paste text into Find in page textfield.
- (void)pasteTextToFindInPage:(NSString*)text {
  [ChromeEarlGrey copyTextToPasteboard:text];
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PasteButton()] performAction:grey_tap()];
}

// Clear text in Find in Page text field.
- (void)clearFindInPageText {
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      performAction:grey_replaceText(@"")];
}

// Matcher for find in page textfield.
- (id<GREYMatcher>)findInPageInputField {
  return grey_accessibilityID(@(kFindInPageSearchFieldID));
}

// Asserts that there is a string "`resultIndex` of `resultCount`" present in
// the results count label. Waits for up to 1 second for this to happen.
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
      base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForActionTimeout, condition),
      @"Timeout waiting for correct Find in Page results string to appear");
}

// Asserts that there is a string "0 of 0" present in the results count label,
// or that the label is not visible. Waits for up to 1 second for this to
// happen.
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
      base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForActionTimeout, condition),
      @"Timeout waiting for correct Find in Page results string to appear");
}

// Asserts that there is a string in the results count label, that is not "0 of
// 0". Waits for up to 1 second for this to happen.
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
      base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForActionTimeout, condition),
      @"Timeout waiting for correct Find in Page results string to appear");
}

// Taps Next button in Find in page.
- (void)advanceToNextResult {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@(kFindInPageNextButtonID))]
      performAction:grey_tap()];
}

// Taps Previous button in Find in page.
- (void)advanceToPreviousResult {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @(kFindInPagePreviousButtonID))]
      performAction:grey_tap()];
}

// Sets up two test servers to test Find in Page on web pages which might
// contain cross-origin iframes.
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

- (void)setUpTestServerForPDFTest {
  // This is sufficient to ensure `ios/testing/data/http_server_files/` file
  // system directory is being served, as this is the default configuration.
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Second test server so cross-origin iframes can be tested together with
// ChromeTestCase's `testServer`.
- (net::test_server::EmbeddedTestServer*)secondTestServer {
  return _secondTestServer.get();
}
// Matcher similar to `grey_text` but more generic i.e. only looks at `hasText`
// prefix.
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

#pragma mark - Tests

// Tests that FIP can be opened with Overflow menu.
- (void)testFindInPageFromOverflowMenu {
  [self setUpTestServersForWebPageTest];

  // Load test page.
  GURL destinationURL = self.testServer->GetURL(kFindInPageTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP with Overflow menu and check it is visible.
  [self openFindInPageWithOverflowMenu];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:[self findInPageInputField]];
  [self closeFindInPageWithDoneButton];
}

// Tests that characters appear in the search box and that results UI updates as
// each characters is entered/deleted.
- (void)testFindInPageTextInput {
  [self setUpTestServersForWebPageTest];

  // Load test page.
  GURL destinationURL = self.testServer->GetURL(kFindInPageTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP.
  [self openFindInPageWithOverflowMenu];
  // Test the result string is empty or "0".
  [self assertResultStringIsEmptyOrZero];

  [self replaceFindInPageText:@(kFindInPageTestRepeatingText)];
  // Test the input field contains the text that was just typed.
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      assertWithMatcher:[self matcherForText:@(kFindInPageTestRepeatingText)]];
  // Test the result UI is updated accordingly.
  [self assertResultStringIsResult:1 outOfTotal:2];

  [self
      replaceFindInPageText:[NSString
                                stringWithFormat:@"%s%s",
                                                 kFindInPageTestRepeatingText,
                                                 kFindInPageTestRepeatingText]];
  [self assertResultStringIsResult:1 outOfTotal:1];

  [self
      replaceFindInPageText:[NSString
                                stringWithFormat:@"%s%s%s",
                                                 kFindInPageTestRepeatingText,
                                                 kFindInPageTestRepeatingText,
                                                 kFindInPageTestRepeatingText]];
  [self assertResultStringIsEmptyOrZero];

  [self clearFindInPageText];
  [self assertResultStringIsEmptyOrZero];
  [self closeFindInPageWithDoneButton];
}

// Tests that the number of results for a query accounts for all the matches
// across frames, here with a main frame and a cross-origin iframe.
- (void)testFindInPageSupportsCrossOriginFrame {
  [self setUpTestServersForWebPageTest];

  // Load test page with cross-origin iframe.
  GURL destinationURL =
      [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP.
  [self openFindInPageWithOverflowMenu];
  [self assertResultStringIsEmptyOrZero];

  [self replaceFindInPageText:@(kFindInPageTestShortText)];
  // Tests there are two matches: one is in the main frame, the other in the
  // cross-origin iframe.
  [self assertResultStringIsResult:1 outOfTotal:2];

  [self advanceToNextResult];
  // Tests that the second match can be navigated to.
  [self assertResultStringIsResult:2 outOfTotal:2];
  [self closeFindInPageWithDoneButton];
}

// Tests that FIP can find different types of characters: special characters,
// number, strings with both letters and numbers as well as non-ASCII
// characters.
- (void)testFindInPageSpecialCharacters {
  [self setUpTestServersForWebPageTest];

  // Load test page with cross-origin iframe.
  GURL destinationURL =
      [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP.
  [self openFindInPageWithOverflowMenu];
  [self assertResultStringIsEmptyOrZero];

  // Tests special characters.
  [self replaceFindInPageText:@(kFindInPageTestSpecialCharactersText)];
  [self assertResultStringIsResult:1 outOfTotal:2];

  // Tests numbers.
  [self replaceFindInPageText:@(kFindInPageTestNumbersText)];
  [self assertResultStringIsResult:1 outOfTotal:2];

  // Tests alphanumeric values.
  [self replaceFindInPageText:@(kFindInPageTestAlphanumericText)];
  [self assertResultStringIsResult:1 outOfTotal:2];

  // Tests non-ASCII characters.
  [self replaceFindInPageText:@(kFindInPageTestNonASCIIText)];
  [self assertResultStringIsResult:1 outOfTotal:2];
  [self closeFindInPageWithDoneButton];
}

// Tests that text can be copied from the web page and pasted into the FIP input
// field and that the results UI updates accordingly.
- (void)testFindInPageCopyPaste {
  // TODO(crbug.com/360362288): Flaky on iOS 18 simulators.
#if TARGET_OS_SIMULATOR
  if (@available(iOS 18, *)) {
    EARL_GREY_TEST_DISABLED(@"Flaky on iOS 18 simulators.");
  }
#endif
  [self setUpTestServersForWebPageTest];

  // Load test page.
  GURL destinationURL = self.testServer->GetURL(kFindInPageTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Select and copy text on the web page.
  [ChromeEarlGreyUI
      longPressElementOnWebView:
          [ElementSelector selectorWithElementID:kFindInPageTestShortTextID]];

  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::SystemSelectionCalloutCopyButton(),
                     grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Open FIP.
  [self openFindInPageWithOverflowMenu];

  // Paste content of pasteboard in the FIP text field.
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PasteButton()] performAction:grey_tap()];

  // Tests that the number of results is updated accordingly.
  [self assertResultStringIsResult:1 outOfTotal:1];
  [self closeFindInPageWithDoneButton];
}

// Tests that FIP yields no results for an empty search query.
- (void)testFindInPageEmptySearchQuery {
  [self setUpTestServersForWebPageTest];

  // Load test page with cross-origin iframe.
  GURL destinationURL =
      [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP.
  [self openFindInPageWithOverflowMenu];

  // Assert that searching text from the page yields results.
  [self replaceFindInPageText:@(kFindInPageTestShortText)];
  [self assertResultStringIsNonZero];

  // Test that the number of results is zero after clearing the FIP text
  // field.
  [self clearFindInPageText];
  [self assertResultStringIsEmptyOrZero];
  [self closeFindInPageWithDoneButton];
}

// Tests that FIP yields no results for a non-empty query with no matches in the
// page.
- (void)testFindInPageQueryWithNoMatches {
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
  [self replaceFindInPageText:@(queryWithNoMatches)];
  // Test the result label shows no results.
  [self assertResultStringIsEmptyOrZero];
  [self closeFindInPageWithDoneButton];
}

// Tests that FIP yields no matches for a text with spanish accents e.g. 'á' if
// the web page contains the same text without spanish accents e.g. 'a'. This
// test assumes removing accents from `kFindInPageTestWithSpanishAccentText`
// yields `kFindInPageTestWithoutSpanishAccentText`.
- (void)testFindInPageDifferentAccent {
  // TODO(crbug.com/439548043): Re-enable the test on iOS26.
  if (base::ios::IsRunningOnIOS26OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.");
  }
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
  [self replaceFindInPageText:@(kFindInPageTestWithoutSpanishAccentText)];
  [self assertResultStringIsNonZero];

  // Replace the text without spanish accent with the same text with spanish
  // accents and test that there are no more matches.
  [self replaceFindInPageText:@(kFindInPageTestWithSpanishAccentText)];
  [self assertResultStringIsEmptyOrZero];
  [self closeFindInPageWithDoneButton];
}

// Test that there is no query persistence with this variant of Native Find in
// Page i.e. with Find interaction.
- (void)testFindInPageHistory {
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
  [self replaceFindInPageText:@(kFindInPageTestShortText)];
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      assertWithMatcher:[self matcherForText:@(kFindInPageTestShortText)]];
  [self closeFindInPageWithDoneButton];

  // Open FIP again and test depending on query persistence.
  [self openFindInPageWithOverflowMenu];
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      assertWithMatcher:[self matcherForText:@""]];
  [self closeFindInPageWithDoneButton];

  // Open the same URL in a different non-Incognito tab.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP in this new tab and test depending on query persistence.
  [self openFindInPageWithOverflowMenu];
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      assertWithMatcher:[self matcherForText:@""]];
  [self closeFindInPageWithDoneButton];
}

// Tests that there is no query persistence from an non-Incognito to an
// Incognito tab.
- (void)testFindInPageNormalToIncognito {
  [self setUpTestServersForWebPageTest];

  // Load test page with cross-origin iframe.
  GURL destinationURL =
      [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP and type short query.
  [self openFindInPageWithOverflowMenu];
  [self replaceFindInPageText:@(kFindInPageTestShortText)];
  [self closeFindInPageWithDoneButton];

  // Load same URL in a new Incognito tab.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP and test the input field is empty.
  [self openFindInPageWithOverflowMenu];
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      assertWithMatcher:[self matcherForText:@""]];
  [self closeFindInPageWithDoneButton];
}

// Tests that switching orientation during a Find session does not throw away
// the query or the current results.
- (void)testFindInPageSwitchOrientation {
  if (base::ios::IsRunningOnIOS26OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.");
  }
  [self setUpTestServersForWebPageTest];

  // Load test page with cross-origin iframe.
  GURL destinationURL =
      [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP, type short query, move to second match and wait for expected
  // results.
  [self openFindInPageWithOverflowMenu];
  [self replaceFindInPageText:@(kFindInPageTestShortText)];
  [self advanceToNextResult];
  [self assertResultStringIsResult:2 outOfTotal:2];

  // Switch to landscape.
  GREYAssert(
      [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationLandscapeLeft
                                       error:nil],
      @"Could not rotate device to Landscape Left");

  // Test the query is still there will the same result.
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      assertWithMatcher:[self matcherForText:@(kFindInPageTestShortText)]];
  [self assertResultStringIsResult:2 outOfTotal:2];

  // Switch back to portrait.
  GREYAssert(
      [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationPortrait
                                       error:nil],
      @"Could not rotate device to Portrait");

  // Test the query is still there will the same result.
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      assertWithMatcher:[self matcherForText:@(kFindInPageTestShortText)]];
  [self assertResultStringIsResult:2 outOfTotal:2];
  [self closeFindInPageWithDoneButton];
}

// Tests that Next/Previous buttons work and wrap.
- (void)testFindInPageNextPreviousArrows {
  [self setUpTestServersForWebPageTest];

  // Load test page with cross-origin iframe.
  GURL destinationURL =
      [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP and type query with four expected matches.
  [self openFindInPageWithOverflowMenu];
  [self replaceFindInPageText:@(kFindInPageTestRepeatingText)];
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
  [self closeFindInPageWithDoneButton];
}

// Tests the various ways to dismiss the keyboard during a Find session.
// TODO(crbug.com/40283787): Test fails on downstream bots.
- (void)DISABLED_testFindInPageDismissKeyboard {
  [self setUpTestServersForWebPageTest];

  // Load test page with cross-origin iframe.
  GURL destinationURL =
      [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP and type short query.
  [self openFindInPageWithOverflowMenu];
  [self replaceFindInPageText:@(kFindInPageTestShortText)];

  // Tap Done button and test the keyboard is dismissed as a result.
  [self closeFindInPageWithDoneButton];
  [ChromeEarlGrey waitForKeyboardToDisappear];

  // Open FIP and type short query again.
  [self openFindInPageWithOverflowMenu];
  [self replaceFindInPageText:@(kFindInPageTestShortText)];

  // Tap an element on the page and test the keyboard is dismissed as a
  // result.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(
                        kFindInPageTestShortTextID)];
  [ChromeEarlGrey waitForKeyboardToDisappear];
}

// Tests that FIP can find long strings of characters.
- (void)testFindInPageLongString {
  [self setUpTestServersForWebPageTest];

  // Load test page with cross-origin iframe.
  GURL destinationURL =
      [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP and type short query.
  [self openFindInPageWithOverflowMenu];
  [self replaceFindInPageText:@(kFindInPageTestLongText)];

  // Test the number of results is as expected.
  [self assertResultStringIsResult:1 outOfTotal:2];
  [self closeFindInPageWithDoneButton];
}

// Tests that FIP is not case sensitive.
- (void)testFindInPageNotCaseSensitive {
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
  [self replaceFindInPageText:[@(kFindInPageTestLowercaseAndUppercaseText)
                                  lowercaseString]];
  // Test the number of results is as expected.
  [self assertResultStringIsResult:1 outOfTotal:2];

  // Clear input field and type uppercase version of contained text.
  [self replaceFindInPageText:[@(kFindInPageTestLowercaseAndUppercaseText)
                                  uppercaseString]];
  // Test the number of results is as expected.
  [self assertResultStringIsResult:1 outOfTotal:2];
  [self closeFindInPageWithDoneButton];
}

// Tests that there is no leak of the FIP search query from Incognito tabs to
// normal tabs.
- (void)testFindInPageIncognitoHistory {
  [self setUpTestServersForWebPageTest];

  // Load test page with cross-origin iframe in new Incognito tab.
  [ChromeEarlGrey openNewIncognitoTab];
  GURL destinationURL =
      [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP and type short query.
  [self openFindInPageWithOverflowMenu];
  [self replaceFindInPageText:@(kFindInPageTestShortText)];
  [self closeFindInPageWithDoneButton];

  // Open a new normal tab and load the same URL.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP again and test the input field is empty.
  [self openFindInPageWithOverflowMenu];
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      assertWithMatcher:[self matcherForText:@""]];
  [self closeFindInPageWithDoneButton];
}

// Tests that there is no query persistence when coming back to a normal tab
// after switching temporarily to another tab.
// TODO(crbug.com/40940589): Re-enable this test.
- (void)FLAKY_testFindInPageSwitchingTabs {
  // TODO(crbug.com/40922941): Failing on iOS17 iPhone.
  if (@available(iOS 17.0, *)) {
    if (![ChromeEarlGrey isIPadIdiom]) {
      XCTSkip(@"Failing on iOS17 iPhone");
    }
  }

  [self setUpTestServersForWebPageTest];

  // Load test page with cross-origin iframe in a second normal tab.
  [ChromeEarlGrey openNewTab];
  GURL destinationURL =
      [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP and type short query.
  [self openFindInPageWithOverflowMenu];
  [self replaceFindInPageText:@(kFindInPageTestShortText)];

  // Switching to first tab and then back to second tab.
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey selectTabAtIndex:1];

  // Test query persistence.
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      assertWithMatcher:[self matcherForText:@""]];
  [self closeFindInPageWithDoneButton];
}

// Tests that FIP can find RTL text in a web page.
// TODO(crbug.com/366752786): Re-enable once de-flaked.
- (void)FLAKY_testFindInPageRTL {
  [self setUpTestServersForWebPageTest];

  // Load test page with cross-origin iframe.
  GURL destinationURL =
      [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP, type RTL text and test that the results are as expected.
  [self openFindInPageWithOverflowMenu];
  [self pasteTextToFindInPage:@(kFindInPageTestRTLText)];
  [self assertResultStringIsResult:1 outOfTotal:2];
  [self closeFindInPageWithDoneButton];
}

// Tests that Find in Page can find matches in an Incognito tab.
- (void)testFindInPageIncognito {
  [self setUpTestServersForWebPageTest];

  // Load test page with cross-origin iframe in a new Incognito tab.
  [ChromeEarlGrey openNewIncognitoTab];
  GURL destinationURL =
      [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP, type text contained in test page and test that the results are
  // as expected.
  [self openFindInPageWithOverflowMenu];
  [self replaceFindInPageText:@(kFindInPageTestShortText)];
  [self assertResultStringIsResult:1 outOfTotal:2];
  [self closeFindInPageWithDoneButton];
}

// Tests accessibility of the Find in Page screen.
- (void)testFindInPageAccessibility {
  [self setUpTestServersForWebPageTest];

  // Load test page with cross-origin iframe.
  GURL destinationURL =
      [self secondTestServer]->GetURL(kFindInPageCrossOriginFrameTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP, type query and check the expected number of results.
  [self openFindInPageWithOverflowMenu];
  [self replaceFindInPageText:@(kFindInPageTestShortText)];
  [self assertResultStringIsResult:1 outOfTotal:2];

  // Test accessibility.
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeFindInPageWithDoneButton];
}

// Tests that Native Find in Page works as expected for PDF documents.
- (void)testFindInPagePDF {
  // TODO(crbug.com/437314322): Re-enable the test on iOS26.
  if (base::ios::IsRunningOnIOS26OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.");
  }

  [self setUpTestServerForPDFTest];

  // Load test PDF document.
  GURL destinationURL = self.testServer->GetURL(kFindInPageComplexPDFTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  // Open FIP and test that the input field is empty and there are no results.
  [self openFindInPageWithOverflowMenu];
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      assertWithMatcher:[self matcherForText:@""]];
  [self assertResultStringIsEmptyOrZero];

  // Type text with 18 expected matches and test that results are as
  // expected.
  [self replaceFindInPageText:@"the F"];
  [self assertResultStringIsResult:1 outOfTotal:18];

  // Test that the Next button works.
  [self advanceToNextResult];
  [self assertResultStringIsResult:2 outOfTotal:18];
  [self advanceToNextResult];
  [self assertResultStringIsResult:3 outOfTotal:18];

  // Type more specific query and test that results are as expected.
  [self replaceFindInPageText:@"the Form"];
  [self assertResultStringIsResult:1 outOfTotal:6];

  // Test that the Previous button works and wraps.
  [self advanceToPreviousResult];
  [self assertResultStringIsResult:6 outOfTotal:6];
  [self advanceToPreviousResult];
  [self assertResultStringIsResult:5 outOfTotal:6];

  // Type even more specific query and test that results are as expected.
  [self replaceFindInPageText:@"the Form 1050"];
  [self assertResultStringIsEmptyOrZero];

  // Test that the Done button does close Find in Page.
  [self closeFindInPageWithDoneButton];
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      assertWithMatcher:grey_notVisible()];
}

// Tests that FIP exit fullscreen when done.
- (void)testWhenFullscreenIsDisable {
  AppLaunchConfiguration config = self.appConfigurationForTestCase;
  // Relaunch the app to take the configuration into account.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

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
  [self openFindInPageWithOverflowMenu];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:[self findInPageInputField]];

  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:chrome_test_util::
                                                             TabShareButton()];

  // Close find in page with Done button and ensure the share button is
  // visible again.
  [self closeFindInPageWithDoneButton];
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that FIP works properly with bottom omnibox.
- (void)testWithBottomOmnibox {
// TODO(crbug.com/437314322): Re-enable the test.
#if !TARGET_OS_SIMULATOR
  if (base::ios::IsRunningOnIOS26OrLater()) {
    if (![ChromeEarlGrey isIPadIdiom]) {
      EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.");
    }
  }
#endif

  // Set bottom Omnibox.
  [ChromeEarlGrey setBoolValue:YES forLocalStatePref:prefs::kBottomOmnibox];

  // Load test page.
  [self setUpTestServersForWebPageTest];
  GURL destinationURL = self.testServer->GetURL(kFindInPageTestURL);
  [ChromeEarlGrey loadURL:destinationURL];

  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  // Open FIP with Overflow menu and check it is visible and the share button
  // is not visible.
  [self openFindInPageWithOverflowMenu];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:[self findInPageInputField]];

  // Hide keyboard.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  // Scroll up and down the page.
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 150)];
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionUp, 150)];

  // Ensure that the bottom Omnibox is not visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxAtBottom()]
      assertWithMatcher:grey_notVisible()];
}

@end
