// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kFirstFragmentText[] = "Hello foo!";
const char kSecondFragmentText[] = "bar";
const char kTestPageTextSample[] = "Lorem ipsum";

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
  return [ChromeEarlGrey executeJavaScript:js];
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
  return [ChromeEarlGrey executeJavaScript:js];
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
  config.features_enabled.push_back(web::features::kScrollToTextIOS);
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

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (void)loadURLPath:(const char*)path {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(path)];
}

// Tests that navigating to a URL with text fragments will highlight all
// fragments.
- (void)testHighlightAllFragments {
  [self loadURLPath:kURLWithTwoFragments];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

  NSArray<NSString*>* markedText = GetMarkedText();

  GREYAssertEqual(2, markedText.count,
                  @"Did not get the expected number of marked text.");
  GREYAssertEqual(kFirstFragmentText, base::SysNSStringToUTF8(markedText[0]),
                  @"First marked text is not valid.");
  GREYAssertEqual(kSecondFragmentText, base::SysNSStringToUTF8(markedText[1]),
                  @"Second marked text is not valid.");
}

// Tests that a fragment will be scrolled to if it's lower on the page.
- (void)testScrollToHighlight {
  [self loadURLPath:kLongPageURLWithOneFragment];
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
                 waitWithTimeout:base::test::ios::kWaitForJSCompletionTimeout],
             @"Could not find visible marked element.");

  GREYAssertEqual(kFirstFragmentText, base::SysNSStringToUTF8(firstVisibleMark),
                  @"Visible marked text is not valid.");
}

@end
