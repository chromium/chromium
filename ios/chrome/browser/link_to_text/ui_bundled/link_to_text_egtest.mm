// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <vector>

#import "base/ios/ios_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/shared_highlighting/core/common/fragment_directives_utils.h"
#import "components/shared_highlighting/core/common/text_fragment.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/browser_container/edit_menu_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_actions_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/element_selector.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"

using shared_highlighting::TextFragment;

namespace {

const char kFirstFragmentText[] = "Hello foo!";
const char kSecondFragmentText[] = "bar";
const char kTestPageTextSample[] = "Lorem ipsum";
const char kNoTextTestPageTextSample[] = "only boundary";
const char kInputTestPageTextSample[] = "has an input";
const char kSimpleTextElementId[] = "toBeSelected";
const char kToBeSelectedText[] = "VeryUniqueWord";

const char kTestURL[] = "/testPage";
const char kURLWithTwoFragments[] = "/testPage/#:~:text=Hello%20foo!&text=bar";
const char kHTMLOfTestPage[] =
    "<html><body>"
    "<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
    "eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Hello foo! Ut enim ad "
    "minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip "
    "ex ea "
    "commodo consequat. bar</p>"
    "<p id=\"toBeSelected\">VeryUniqueWord</p>"
    "</body></html>";

const char kTestLongPageURL[] = "/longTestPage";
const char kLongPageURLWithOneFragment[] =
    "/longTestPage/#:~:text=Hello%20foo!";
const char kHTMLOfLongTestPage[] =
    "<html><body>"
    "<div style=\"background:blue; height: 4000px; width: 250px;\"></div>"
    "<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
    "eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Hello foo! Ut enim ad "
    "minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip "
    "ex ea "
    "commodo consequat. bar</p>"
    "<div style=\"background:blue; height: 4000px; width: 250px;\"></div>"
    "</body></html>";

const char kNoTextTestURL[] = "/noTextPage";
const char kHTMLOfNoTextTestPage[] =
    "<html><body>"
    "This page has a paragraph with only boundary characters"
    "<p id=\"toBeSelected\"> .!, \t</p>"
    "</body></html>";

const char kInputTestURL[] = "/inputTextPage";
const char kHTMLOfInputTestPage[] =
    "<html><body>"
    "This page has an input"
    "<input type=\"text\" id=\"toBeSelected\"></p>"
    "</body></html>";

NSArray<NSString*>* GetMarkedText() {
  NSString* js = @"(function() {"
                  "  const marks = document.getElementsByTagName('mark');"
                  "  const markedText = [];"
                  "  for (const mark of marks) {"
                  "    if (mark && mark.innerText) {"
                  "      markedText.push(mark.innerText);"
                  "    }"
                  "  }"
                  "  return markedText;"
                  "})();";
  base::Value result = [ChromeEarlGrey evaluateJavaScript:js];
  GREYAssertTrue(result.is_list(), @"Result is not iterable.");

  NSMutableArray<NSString*>* marked_texts = [NSMutableArray array];
  for (const auto& element : result.GetList()) {
    if (element.is_string()) {
      NSString* ns_element = base::SysUTF8ToNSString(element.GetString());
      [marked_texts addObject:ns_element];
    }
  }

  return [marked_texts copy];
}

NSString* GetFirstVisibleMarkedText() {
  NSString* js =
      @"(function () {"
       "  const firstMark = document.getElementsByTagName('mark')[0];"
       "  if (!firstMark) {"
       "    return '';"
       "  }"
       "  const rect = firstMark.getBoundingClientRect();"
       "  const isVisible = rect.top >= 0 &&"
       "    rect.bottom <= window.innerHeight &&"
       "    rect.left >= 0 &&"
       "    rect.right <= window.innerWidth;"
       "  return isVisible ? firstMark.innerText : '';"
       "})();";
  base::Value result = [ChromeEarlGrey evaluateJavaScript:js];
  GREYAssertTrue(result.is_string(), @"Result is not a string.");
  return base::SysUTF8ToNSString(result.GetString());
}

std::unique_ptr<net::test_server::HttpResponse> LoadHtml(
    const std::string& html,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_content_type("text/html");
  http_response->set_content(html);
  return std::move(http_response);
}

}  // namespace

// Test class for the scroll-to-text and link-to-text features.
@interface LinkToTextTestCase : ChromeTestCase
@end

@implementation LinkToTextTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kSharedHighlightingIOS);
  return config;
}

- (void)setUp {
  [super setUp];

  RegisterDefaultHandlers(self.testServer);
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&net::test_server::HandlePrefixedRequest, kTestURL,
                          base::BindRepeating(&LoadHtml, kHTMLOfTestPage)));
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kTestLongPageURL,
      base::BindRepeating(&LoadHtml, kHTMLOfLongTestPage)));
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kNoTextTestURL,
      base::BindRepeating(&LoadHtml, kHTMLOfNoTextTestPage)));
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kInputTestURL,
      base::BindRepeating(&LoadHtml, kHTMLOfInputTestPage)));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that navigating to a URL with text fragments will highlight all
// fragments.
- (void)testHighlightAllFragments {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kURLWithTwoFragments)];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

  NSArray<NSString*>* markedText = GetMarkedText();

  GREYAssertEqual(2, markedText.count,
                  @"Did not get the expected number of marked text.");
  GREYAssertEqualObjects(@(kFirstFragmentText), markedText[0],
                         @"First marked text is not valid.");
  GREYAssertEqualObjects(@(kSecondFragmentText), markedText[1],
                         @"Second marked text is not valid.");
}

// Tests that a fragment will be scrolled to if it's lower on the page.
- (void)testScrollToHighlight {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kLongPageURLWithOneFragment)];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

  __block NSString* firstVisibleMark;
  GREYCondition* scrolledToText = [GREYCondition
      conditionWithName:@"Did not scroll to marked text."
                  block:^{
                    NSString* visibleMarkedText = GetFirstVisibleMarkedText();
                    if (visibleMarkedText &&
                        visibleMarkedText != [NSString string]) {
                      firstVisibleMark = visibleMarkedText;
                      return YES;
                    }
                    return NO;
                  }];

  GREYAssert([scrolledToText
                 waitWithTimeout:base::test::ios::kWaitForJSCompletionTimeout
                                     .InSecondsF()],
             @"Could not find visible marked element.");

  GREYAssertEqualObjects(@(kFirstFragmentText), firstVisibleMark,
                         @"Visible marked text is not valid.");
}

// Tests that a link can be generated for a simple text selection.
// crbug.com/1403831 Disable flaky test
- (void)DISABLED_testGenerateLinkForSimpleText {
  [ChromeEarlGrey clearPasteboard];
  GURL pageURL = self.testServer->GetURL(kTestURL);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector
                            selectorWithElementID:kSimpleTextElementId],
                        true)];

  // Wait for the menu to open. The "Copy" menu item will always be present,
  // but other items may be hidden behind the overflow button.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      chrome_test_util::SystemSelectionCalloutCopyButton()];

  // The link to text button may be in the overflow, so use a search action to
  // find it, if necessary.
  id<GREYMatcher> linkToTextMatcher =
      grey_allOf(chrome_test_util::SystemSelectionCalloutLinkToTextButton(),
                 grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:linkToTextMatcher]
         usingSearchAction:grey_tap()
      onElementWithMatcher:chrome_test_util::
                               SystemSelectionCalloutOverflowButton()]
      performAction:grey_tap()];

  // Make sure the Edit menu is gone.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SystemSelectionCallout()]
      assertWithMatcher:grey_notVisible()];

  // Wait for the Activity View to show up (look for the Copy action).
  id<GREYMatcher> copyActivityButton = chrome_test_util::CopyActivityButton();
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:copyActivityButton];

  // Tap on the Copy action.
  [[EarlGrey selectElementWithMatcher:copyActivityButton]
      performAction:grey_tap()];

  // Assert the values stored in the pasteboard. Lower-casing the expected
  // GURL as that is what the JS library is doing.
  NSString* stringURL = base::SysUTF8ToNSString(pageURL.spec());
  NSString* fragment = @"#:~:text=bar-,";
  NSString* selectedText =
      base::SysUTF8ToNSString(base::ToLowerASCII(kToBeSelectedText));

  NSString* expectedURL =
      [NSString stringWithFormat:@"%@%@%@", stringURL, fragment, selectedText];
  [ChromeEarlGrey verifyStringCopied:expectedURL];

  [ChromeEarlGrey clearPasteboard];
}

- (void)testBadSelectionDisablesGenerateLink {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kNoTextTestURL)];
  [ChromeEarlGrey waitForWebStateContainingText:kNoTextTestPageTextSample];

  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector
                            selectorWithElementID:kSimpleTextElementId],
                        true)];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:[EditMenuAppInterface
                                                       editMenuMatcher]];

  // Make sure the Link to Text button is not visible.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::SystemSelectionCalloutLinkToTextButton()]
      assertWithMatcher:grey_notVisible()];

  // TODO(crbug.com/40191349): Tap to dismiss the system selection callout
  // buttons so tearDown doesn't hang when `disabler` goes out of scope.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:grey_tap()];
}

- (void)testInputDisablesGenerateLink {
  // In order to make the menu show up later in the test, the pasteboard can't
  // be empty.
  UIPasteboard* pasteboard = [UIPasteboard generalPasteboard];
  pasteboard.string = @"anything";

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kInputTestURL)];
  [ChromeEarlGrey waitForWebStateContainingText:kInputTestPageTextSample];

  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Tap to focus the field, then long press to make the Edit Menu pop up.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(
                        kSimpleTextElementId)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector
                            selectorWithElementID:kSimpleTextElementId],
                        true)];

  // TODO(crbug.com/40191349): Xcode 13 gesture recognizers seem to get stuck
  // when the user longs presses on plain text.  For this test, disable EG
  // synchronization.
  ScopedSynchronizationDisabler disabler;

  // Ensure the menu is visible by finding the Paste button.
  // TODO(crbug.com/328271981): either remove call to selectElementWithMatcher
  // or do something with its return value
  // id<GREYMatcher> menu = grey_accessibilityLabel(@"Paste");
  // [EarlGrey selectElementWithMatcher:menu];

  // Make sure the Link to Text button is not visible.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::SystemSelectionCalloutLinkToTextButton()]
      assertWithMatcher:grey_notVisible()];

  // TODO(crbug.com/40191349): Tap to dismiss the system selection callout
  // buttons so tearDown doesn't hang when `disabler` goes out of scope.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:grey_tap()];
}

@end
