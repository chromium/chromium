// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#import "components/handoff/handoff_manager.h"
#import "ios/chrome/browser/device_sharing/device_sharing_manager.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Checks that Handoff will report the specified |gurl|.
void AssertHandoffURL(const GURL& gurl) {
  HandoffManager* manager =
      [chrome_test_util::GetDeviceSharingManager() handoffManager];
  GREYAssertTrue(manager != nil, @"Handoff Manager should not be nil");
  if (gurl.is_valid()) {
    NSURL* URL = net::NSURLWithGURL(gurl);
    GREYAssertTrue([manager.userActivityWebpageURL isEqual:URL],
                   @"Incorrect Handoff URL.");
  } else {
    GREYAssertTrue(manager.userActivityWebpageURL == nil,
                   @"Handoff URL is not nil.");
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
  chrome_test_util::OpenNewTab();
  const GURL destinationUrl = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];
  AssertHandoffURL(destinationUrl);
}

// Tests that Handoff URL should never be set for an incognito tab.
- (void)testTypicalURLInNewIncognitoTab {
  // Opens an incognito tab and loads a web page. Check that Handoff URL is nil.
  chrome_test_util::OpenNewIncognitoTab();
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
  chrome_test_util::OpenNewTab();
  [ChromeEarlGrey loadURL:tab2URL];
  chrome_test_util::OpenNewTab();
  [ChromeEarlGrey loadURL:tab3URL];

  // When tab 3 is closed, tab 2 is front and Handoff URL should be the URL for
  // tab 2.
  chrome_test_util::CloseCurrentTab();
  AssertHandoffURL(tab2URL);

  // Switches back to the first tab.
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);
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
  chrome_test_util::OpenNewIncognitoTab();
  [ChromeEarlGrey loadURL:tab2URL];
  AssertHandoffURL(GURL());

  // Loads page three in a new normal tab and verify that Handoff URL is not
  // nil.
  chrome_test_util::OpenNewTab();
  [ChromeEarlGrey loadURL:tab3URL];
  AssertHandoffURL(tab3URL);
}

@end
