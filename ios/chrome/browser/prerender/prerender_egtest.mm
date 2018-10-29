// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/omnibox/truncating_attributed_label.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/test/app/history_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kPageURL[] = "/test-page.html";
const char kPageTitle[] = "Page title!";
const char kPageLoadedString[] = "Page loaded!";

// Provides responses for redirect and changed window location URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
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
  return std::move(http_response);
}
}  // namespace

// Test case for the prerender.
@interface PrerenderTestCase : ChromeTestCase

@end

@implementation PrerenderTestCase

// Test that tapping the prerendered suggestions opens it.
- (void)testTapPrerenderSuggestions {
  // TODO(crbug.com/793306): Re-enable the test on iOS 11 iPad once the
  // alternate letters problem is fixed.
  if (IsIPadIdiom()) {
    if (@available(iOS 11, *)) {
      EARL_GREY_TEST_DISABLED(
          @"Disabled for iPad due to alternate letters educational screen.");
    }
  }

  GREYAssertTrue(chrome_test_util::ClearBrowsingHistory(),
                 @"Clearing Browsing History timed out");
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  // Set server up.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  NSString* pageString = base::SysUTF8ToNSString(pageURL.GetContent());

  // Go to the page a couple of time so it shows as suggestion.
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey goBack];
  [[self class] closeAllTabs];
  chrome_test_util::OpenNewTab();
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   ntp_home::FakeOmniboxAccessibilityID())]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForElementWithMatcherSufficientlyVisible:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText([pageString stringByAppendingString:@"\n"])];
  [ChromeEarlGrey waitForPageToFinishLoading];
  [[self class] closeAllTabs];
  chrome_test_util::OpenNewTab();
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Type the begining of the address to have the autocomplete suggestion.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   ntp_home::FakeOmniboxAccessibilityID())]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForElementWithMatcherSufficientlyVisible:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(
                        [pageString substringToIndex:[pageString length] - 6])];

  // Make sure the omnibox is autocompleted.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(pageString),
                                          grey_ancestor(grey_kindOfClass(
                                              [OmniboxTextFieldIOS class])),
                                          nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open the suggestion. The suggestion needs to be the first suggestion to
  // have the prerenderer activated.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(pageString),
                                   grey_kindOfClass(
                                       [OmniboxPopupTruncatingLabel class]),
                                   grey_ancestor(grey_accessibilityID(
                                       @"omnibox suggestion 0")),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
}

@end
