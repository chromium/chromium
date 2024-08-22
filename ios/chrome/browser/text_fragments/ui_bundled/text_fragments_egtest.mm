// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "components/shared_highlighting/core/common/shared_highlighting_features.h"
#import "components/shared_highlighting/ios/shared_highlighting_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const char kTestURL[] = "/testPage";
const char kURLWithFragment[] = "/testPage/#:~:text=lorem%20ipsum";
const char kHTMLOfTestPage[] =
    "<html><body><p>"
    "<span id='target'>Lorem ipsum</span> dolor sit amet, consectetur "
    "adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore "
    "magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco "
    "laboris nisi ut aliquip ex ea commodo consequat.</p>"
    "<a href='/testPage2' id='link1'>Link 1</a>"
    "<a href='#target' id='link2'>Link 2</a>"
    "</body></html>";
const char kTestPageTextSample[] = "Lorem ipsum";

const char kTestURL2[] = "/testPage2";
const char kHTMLOfTestPage2[] =
    "<html><body>Navigated to second page</body></html>";
const char kTestPage2TextSample[] = "Navigated to second page";

std::unique_ptr<net::test_server::HttpResponse> LoadHtml(
    const std::string& html,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_content_type("text/html");
  http_response->set_content(html);
  return std::move(http_response);
}

auto GetMenuTitleMatcher() {
  return grey_text(l10n_util::GetNSString(IDS_IOS_SHARED_HIGHLIGHT_MENU_TITLE));
}

void ClickMarkAndWaitForMenu() {
  ElementSelector* selector = [ElementSelector selectorWithCSSSelector:"mark"];
  [ChromeEarlGrey waitForWebStateContainingElement:selector];
  [ChromeEarlGrey
      evaluateJavaScriptForSideEffect:
          @"document.getElementById('target').children[0].click();"];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:GetMenuTitleMatcher()];
}

void DismissMenu() {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // Tap the tools menu to dismiss the popover.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
        performAction:grey_tap()];
  }
}

void ReshareToPasteboard(const GURL& expected) {
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_SHARED_HIGHLIGHT_RESHARE))]
      performAction:grey_tap()];

  // Tap on the Copy action.
  [ChromeEarlGrey tapButtonInActivitySheetWithID:@"Copy"];

  // Wait for the value to be in the pasteboard.
  GREYCondition* getPastedURL = [GREYCondition
      conditionWithName:@"Could not get expected URL from the pasteboard."
                  block:^{
                    return expected == [ChromeEarlGrey pasteboardURL];
                  }];
  GREYAssert(
      [getPastedURL
          waitWithTimeout:base::test::ios::kWaitForActionTimeout.InSecondsF()],
      @"Could not get expected URL from pasteboard.");
}

}  // namespace

// Test class verifying behavior of interactions with text fragments in web
// pages.
@interface TextFragmentsTestCase : ChromeTestCase
@end

@implementation TextFragmentsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      shared_highlighting::kIOSSharedHighlightingV2);
  return config;
}

- (void)setUp {
  [super setUp];

  RegisterDefaultHandlers(self.testServer);
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&net::test_server::HandlePrefixedRequest, kTestURL,
                          base::BindRepeating(&LoadHtml, kHTMLOfTestPage)));
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&net::test_server::HandlePrefixedRequest, kTestURL2,
                          base::BindRepeating(&LoadHtml, kHTMLOfTestPage2)));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (void)testOpenMenu {
  // TODO(crbug.com/361562688): Fix and re-enable.
  if (@available(iOS 18, *)) {
    EARL_GREY_TEST_DISABLED(@"Failing on iOS 18 simulators.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kURLWithFragment)];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

  ClickMarkAndWaitForMenu();
}

// Disabled test due to multiple builder failures.
// TODO(crbug.com/40214683): re-enable the test with fix.
- (void)DISABLED_testRemove {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kURLWithFragment)];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

  ClickMarkAndWaitForMenu();

  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_SHARED_HIGHLIGHT_REMOVE))]
      performAction:grey_tap()];

  // Verify that the mark is gone
  ElementSelector* selector = [ElementSelector selectorWithCSSSelector:"mark"];
  [ChromeEarlGrey waitForWebStateNotContainingElement:selector];
}

- (void)testCancel {
  // TODO(crbug.com/361562688): Fix and re-enable.
  if (@available(iOS 18, *)) {
    EARL_GREY_TEST_DISABLED(@"Failing on iOS 18 simulators.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kURLWithFragment)];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

  ClickMarkAndWaitForMenu();

  DismissMenu();

  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:GetMenuTitleMatcher()];

  // Verify that the mark is still present
  ElementSelector* selector = [ElementSelector selectorWithCSSSelector:"mark"];
  [ChromeEarlGrey waitForWebStateContainingElement:selector];
}

- (void)testLearnMore {
  // TODO(crbug.com/361562688): Fix and re-enable.
  if (@available(iOS 18, *)) {
    EARL_GREY_TEST_DISABLED(@"Failing on iOS 18 simulators.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kURLWithFragment)];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

  ClickMarkAndWaitForMenu();
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_SHARED_HIGHLIGHT_LEARN_MORE))]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForMainTabCount:2];

  // Compare only the host; the path could change upon opening.
  GREYAssertEqual([ChromeEarlGrey webStateLastCommittedURL].host(),
                  GURL(shared_highlighting::kLearnMoreUrl).host(),
                  @"Did not open correct Learn More URL.");
}

- (void)testReshare {
  // TODO(crbug.com/361562688): Fix and re-enable.
  if (@available(iOS 18, *)) {
    EARL_GREY_TEST_DISABLED(@"Failing on iOS 18 simulators.");
  }

  // Clear the pasteboard
  UIPasteboard* pasteboard = UIPasteboard.generalPasteboard;
  [pasteboard setValue:@"" forPasteboardType:UIPasteboardNameGeneral];

  GURL pageURL = self.testServer->GetURL(kURLWithFragment);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];
  ClickMarkAndWaitForMenu();
  ReshareToPasteboard(pageURL);
}

// Verify that navigating away from the page and then coming back does not
// result in two sets of <mark> elements being created.
- (void)testNoDuplicatesOnNavigation {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kURLWithFragment)];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];
  ElementSelector* selector = [ElementSelector selectorWithCSSSelector:"mark"];
  [ChromeEarlGrey waitForWebStateContainingElement:selector];

  // Click link to navigate away, then return to where we started
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:
                      @"document.getElementById('link1').click();"];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2TextSample];
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

  // Count how many <mark> elements exist in the page. It should be OK to call
  // this now because the JS to create highlights runs as soon as navigation
  // finishes, and JS is single-threaded, so this will be evaluated after that.
  base::Value result = [ChromeEarlGrey
      evaluateJavaScript:@"(function() {"
                          "return document.getElementsByTagName('mark').length;"
                          "})();"];

  // Even though it's a count, we retrieve it as a double because JS numbers are
  // always treated as doubles.
  GREYAssertTrue(result.is_double(), @"Count of mark elements is not a number");
  GREYAssertEqual(1, result.GetDouble(),
                  @"Found wrong number of mark elements");
}

// Verify that navigating away from the page makes the menu go away.
- (void)testMenuDismissesOnNavigation {
  // TODO(crbug.com/361562688): Fix and re-enable.
  if (@available(iOS 18, *)) {
    EARL_GREY_TEST_DISABLED(@"Failing on iOS 18 simulators.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kURLWithFragment)];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

  ClickMarkAndWaitForMenu();

  // Navigation after the menu is already showing should cause it to disappear.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kTestURL2)];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:GetMenuTitleMatcher()];

  // Go back to the original page.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

  // Clicking a link inside a highlight will fire both events at roughly the
  // same time. Verify that the menu either goes away or never shows up to begin
  // with.
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:
                      @"document.getElementById('link1').click();"];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:GetMenuTitleMatcher()];
}

- (void)testReshareWorksAfterNavigation {
  // Clear the pasteboard
  UIPasteboard* pasteboard = UIPasteboard.generalPasteboard;
  [pasteboard setValue:@"" forPasteboardType:UIPasteboardNameGeneral];

  GURL pageURL = self.testServer->GetURL(kURLWithFragment);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

  // Click a link to an anchor in the document
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:
                      @"document.getElementById('link2').click();"];
  GREYCondition* finishedSameDocNavigation = [GREYCondition
      conditionWithName:@"Did not navigate within document."
                  block:^{
                    return [ChromeEarlGrey webStateLastCommittedURL].ref() ==
                           "target";
                  }];
  GREYAssert(
      [finishedSameDocNavigation
          waitWithTimeout:base::test::ios::kWaitForActionTimeout.InSecondsF()],
      @"Did not navigate within document.");

  // When resharing, the text fragments should persist even though we've
  // added a reference fragment.
  GURL expected =
      self.testServer->GetURL("/testPage/#target:~:text=lorem%20ipsum");
  ClickMarkAndWaitForMenu();
  ReshareToPasteboard(expected);

  // When navigating back, the highlights persist even though the committed (and
  // displayed) URL doesn't contain a text fragment. Resharing should still
  // include the text fragments.
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:
                      @"document.getElementById('link1').click();"];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2TextSample];
  [ChromeEarlGrey goBack];

  ClickMarkAndWaitForMenu();
  ReshareToPasteboard(expected);
}

@end
