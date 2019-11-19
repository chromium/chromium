// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>
#import "ios/testing/earl_grey/earl_grey_test.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace {
const char kPageURL[] = "/test-page.html";
const char kPageTitle[] = "Page title!";
const char kPageLoadedString[] = "Page loaded!";

// Provides responses for redirect and changed window location URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    int* counter,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kPageURL) {
    return nullptr;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content("<html><head><title>" + std::string(kPageTitle) +
                             "</title></head><body>" +
                             std::string(kPageLoadedString) + "</body></html>");
  (*counter)++;
  return std::move(http_response);
}
}  // namespace

// Test case for the prerender.
@interface PrerenderTestCase : ChromeTestCase

@end

@implementation PrerenderTestCase

// Test that tapping the prerendered suggestions opens it.
- (void)testTapPrerenderSuggestions {
  // TODO(crbug.com/793306): Re-enable the test on iPad once the alternate
  // letters problem is fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(
        @"Disabled for iPad due to alternate letters educational screen.");
  }

  [ChromeEarlGrey clearBrowsingHistory];
  // Set server up.
  int visitCounter = 0;
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse, &visitCounter));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  NSString* pageString = base::SysUTF8ToNSString(pageURL.GetContent());

  // Go to the page a couple of time so it shows as suggestion.
  [ChromeEarlGrey loadURL:pageURL];
  GREYAssertEqual(1, visitCounter, @"The page should have been loaded once");
  [ChromeEarlGrey goBack];
  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText([pageString stringByAppendingString:@"\n"])];
  [ChromeEarlGrey waitForPageToFinishLoading];
  GREYAssertEqual(2, visitCounter, @"The page should have been loaded twice");
  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];

  // Type the begining of the address to have the autocomplete suggestion.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(
                        [pageString substringToIndex:[pageString length] - 6])];

  // Wait until prerender request reaches the server.
  bool prerendered = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return visitCounter == 3;
  });
  GREYAssertTrue(prerendered, @"Prerender did not happen");

  // Make sure the omnibox is autocompleted.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(pageString),
                                          grey_ancestor(grey_kindOfClassName(
                                              @"OmniboxTextFieldIOS")),
                                          nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open the suggestion. The suggestion needs to be the first suggestion to
  // have the prerenderer activated.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(pageString),
                                   grey_kindOfClassName(@"FadeTruncatingLabel"),
                                   grey_ancestor(grey_accessibilityID(
                                       @"omnibox suggestion 0")),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  GREYAssertEqual(3, visitCounter, @"Prerender should have been the last load");
}

@end
