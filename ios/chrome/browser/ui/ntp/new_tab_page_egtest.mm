// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#include "base/test/scoped_command_line.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/metrics/metrics_app_interface.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kPageLoadedString[] = "Page loaded!";
const char kPageURL[] = "/test-page.html";
const char kPageTitle[] = "Page title!";

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

// Pauses until the history label has disappeared.  History should not show on
// incognito.
BOOL WaitForHistoryToDisappear() {
  return [[GREYCondition
      conditionWithName:@"Wait for history to disappear"
                  block:^BOOL {
                    NSError* error = nil;
                    NSString* history =
                        l10n_util::GetNSString(IDS_HISTORY_SHOW_HISTORY);
                    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                                            history)]
                        assertWithMatcher:grey_notVisible()
                                    error:&error];
                    return error == nil;
                  }] waitWithTimeout:base::test::ios::kWaitForUIElementTimeout];
}

}  // namespace

@interface NewTabPageTestCase : ChromeTestCase
@property(nonatomic, assign) BOOL histogramTesterSet;
@end

@implementation NewTabPageTestCase

- (void)tearDown {
  [self releaseHistogramTester];
  [super tearDown];
}

- (void)setupHistogramTester {
  if (self.histogramTesterSet) {
    return;
  }
  self.histogramTesterSet = YES;
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
}

- (void)releaseHistogramTester {
  if (!self.histogramTesterSet) {
    return;
  }
  self.histogramTesterSet = NO;
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
}

#pragma mark - Tests

// Tests that all items are accessible on the most visited page.
- (void)testAccessibilityOnMostVisited {
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that the NTP is still displayed after loading an invalid URL.
- (void)testNTPStayForInvalidURL {
  if (@available(iOS 13, *)) {
  } else {
    EARL_GREY_TEST_DISABLED(@"Failing on iOS 12.");
  }
// TODO(crbug.com/1067813): Test won't pass on iPad device.
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"This test doesn't pass on iPad device.");
  }
#endif
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_typeText(@"file://\n")];

  // Make sure that the URL disappeared.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText("file://")]
      assertWithMatcher:grey_nil()];

  // Check that the NTP is still displayed (because the fake omnibox is here).
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the metrics are reported correctly.
- (void)testNTPMetrics {
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  [ChromeEarlGrey closeAllTabs];

  // Open and close an NTP.
  [self setupHistogramTester];
  NSError* error =
      [MetricsAppInterface expectTotalCount:0
                               forHistogram:@"NewTabPage.TimeSpent"];
  GREYAssertNil(error, error.description);
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey closeAllTabs];
  error = [MetricsAppInterface expectTotalCount:1
                                   forHistogram:@"NewTabPage.TimeSpent"];
  GREYAssertNil(error, error.description);
  [self releaseHistogramTester];

  // Open an incognito NTP and close it.
  [ChromeEarlGrey closeAllTabs];
  [self setupHistogramTester];
  error = [MetricsAppInterface expectTotalCount:0
                                   forHistogram:@"NewTabPage.TimeSpent"];
  GREYAssertNil(error, error.description);
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey closeAllTabs];
  error = [MetricsAppInterface expectTotalCount:0
                                   forHistogram:@"NewTabPage.TimeSpent"];
  GREYAssertNil(error, error.description);
  [self releaseHistogramTester];

  // Open an NTP and navigate to another URL.
  [self setupHistogramTester];
  error = [MetricsAppInterface expectTotalCount:0
                                   forHistogram:@"NewTabPage.TimeSpent"];
  GREYAssertNil(error, error.description);
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];

  error = [MetricsAppInterface expectTotalCount:1
                                   forHistogram:@"NewTabPage.TimeSpent"];
  GREYAssertNil(error, error.description);
  [self releaseHistogramTester];

  // Open an NTP and switch tab.
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];

  [self setupHistogramTester];
  error = [MetricsAppInterface expectTotalCount:0
                                   forHistogram:@"NewTabPage.TimeSpent"];
  GREYAssertNil(error, error.description);
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey selectTabAtIndex:0];
  error = [MetricsAppInterface expectTotalCount:1
                                   forHistogram:@"NewTabPage.TimeSpent"];
  GREYAssertNil(error, error.description);
  [ChromeEarlGrey selectTabAtIndex:1];
  error = [MetricsAppInterface expectTotalCount:1
                                   forHistogram:@"NewTabPage.TimeSpent"];
  GREYAssertNil(error, error.description);
  [ChromeEarlGrey selectTabAtIndex:0];
  error = [MetricsAppInterface expectTotalCount:2
                                   forHistogram:@"NewTabPage.TimeSpent"];
  GREYAssertNil(error, error.description);
  [self releaseHistogramTester];
}

// Tests that all items are accessible on the incognito page.
- (void)testAccessibilityOnIncognitoTab {
  [ChromeEarlGrey openNewIncognitoTab];
  GREYAssert(WaitForHistoryToDisappear(), @"History did not disappear.");
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [ChromeEarlGrey closeAllIncognitoTabs];
}

@end
