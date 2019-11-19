// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/browser/device_sharing/handoff_manager_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(HandoffManagerAppInterface);
#pragma clang diagnostic pop
#endif  // defined(CHROME_EARL_GREY_2)

namespace {

// Checks that Handoff will report the specified |gurl|.
void AssertHandoffURL(const GURL& gurl) {
  NSURL* handoffURL =
      [HandoffManagerAppInterface currentUserActivityWebPageURL];
  if (gurl.is_valid()) {
    NSURL* URL = net::NSURLWithGURL(gurl);
    GREYAssertTrue([handoffURL isEqual:URL], @"Incorrect Handoff URL.");
  } else {
    GREYAssertTrue(handoffURL == nil, @"Handoff URL is not nil.");
  }
}

}  // namespace

// Tests that HandoffManager reports the correct active URL based on the
// active tab.
@interface HandoffManagerTestCase : ChromeTestCase
@end

@implementation HandoffManagerTestCase

#pragma mark - Overrides base class

- (void)setUp {
  [super setUp];
  web::test::SetUpFileBasedHttpServer();
}

#pragma mark - Tests

// Tests that an empty new tab page should result in no Handoff URL.
- (void)testNewTabPageEmptyURL {
  AssertHandoffURL(GURL());
}

// Tests that the simple case of Handoff URL for a single page.
- (void)testTypicalURL {
  const GURL destinationUrl = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  [ChromeEarlGrey loadURL:destinationUrl];
  AssertHandoffURL(destinationUrl);
}

// Tests Handoff URL for a new tab.
- (void)testTypicalURLInNewTab {
  [ChromeEarlGrey openNewTab];
  const GURL destinationUrl = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];
  AssertHandoffURL(destinationUrl);
}

// Tests that Handoff URL should never be set for an incognito tab.
- (void)testTypicalURLInNewIncognitoTab {
  // Opens an incognito tab and loads a web page. Check that Handoff URL is nil.
  [ChromeEarlGrey openNewIncognitoTab];
  const GURL destinationUrl = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  [ChromeEarlGrey loadURL:destinationUrl];
  AssertHandoffURL(GURL());

  // Loads a second URL on the same incognito tab. Handoff URL should still be
  // nil.
  const GURL destinationUrl2 = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl2];
  AssertHandoffURL(GURL());
}

// Tests the state for Handoff URL when creating, closing tab, and switching
// tab.
- (void)testMultipleSwitchingTabs {
  const GURL tab1URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  const GURL tab2URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  const GURL tab3URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/chromium_logo_page.html");

  // Sets up the state for 3 tabs.
  [ChromeEarlGrey loadURL:tab1URL];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:tab2URL];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:tab3URL];

  // When tab 3 is closed, tab 2 is front and Handoff URL should be the URL for
  // tab 2.
  [ChromeEarlGrey closeCurrentTab];
  AssertHandoffURL(tab2URL);

  // Switches back to the first tab.
  [ChromeEarlGrey selectTabAtIndex:0];
  AssertHandoffURL(tab1URL);
}

// Tests the state for Handoff URL when switching between normal tabs and
// incognito tabs.
- (void)testSwitchBetweenNormalAndIncognitoTabs {
  const GURL tab1URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  const GURL tab2URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  const GURL tab3URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/chromium_logo_page.html");

  // Loads one page.
  [ChromeEarlGrey loadURL:tab1URL];
  // Loads page two in incognito and verifies that Handoff URL is nil.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:tab2URL];
  AssertHandoffURL(GURL());

  // Loads page three in a new normal tab and verify that Handoff URL is not
  // nil.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:tab3URL];
  AssertHandoffURL(tab3URL);
}

@end
