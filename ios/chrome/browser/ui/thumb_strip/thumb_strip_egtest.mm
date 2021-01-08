// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

using chrome_test_util::PrimaryToolbar;

namespace {

// net::EmbeddedTestServer handler that responds with the request's query as the
// title and body.
std::unique_ptr<net::test_server::HttpResponse> HandleQueryTitle(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_content_type("text/html");
  http_response->set_content("<html><head><title>" + request.GetURL().query() +
                             "</title></head><body>" +
                             request.GetURL().query() + "</body></html>");
  return std::move(http_response);
}

}  // namespace

// Thumb Strip tests for Chrome.
@interface ThumbStripTestCase : ChromeTestCase
@end

@implementation ThumbStripTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Enabling this feature causes all tests to fail as part of setup.
  // See crbug.com/1143299.
  if (base::ios::IsRunningOnIOS13OrLater()) {
    config.features_enabled.push_back(kExpandedTabStrip);
    config.features_disabled.push_back(
        fullscreen::features::kSmoothScrollingDefault);
  }
  return config;
}

// Sets up the EmbeddedTestServer as needed for tests.
- (void)setUpTestServer {
  self.testServer->RegisterDefaultHandler(base::BindRepeating(
      net::test_server::HandlePrefixedRequest, "/querytitle",
      base::BindRepeating(&HandleQueryTitle)));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");
}

// Tests that the entire thumb strip is visible in peeked state. Specifically,
// this tests that the thumb strip is not partially covered when Smooth
// Scrolling is on.
- (void)testThumbStripVisibleInPeekedState {
  // The feature only works on iPad.
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Thumb strip is not enabled on iPhone");
  }

  // See crbug.com/1143299.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Fails on iOS 12 devices.");
  }

  [self setUpTestServer];

  const GURL URL = self.testServer->GetURL("/querytitle?Hello");

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Hello"];

  // Swipe down to reveal the thumb strip.
  [[EarlGrey selectElementWithMatcher:PrimaryToolbar()]
      performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];

  // Make sure that the entire tab thumbnail is fully visible and not covered.
  // This acts as a good proxy to the entire thumbstrip being visible.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(@"Hello"),
                                          grey_kindOfClassName(@"GridCell"),
                                          grey_minimumVisiblePercent(1), nil)]
      assertWithMatcher:grey_notNil()];
}

@end
