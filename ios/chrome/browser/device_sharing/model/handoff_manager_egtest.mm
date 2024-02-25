// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/browser/device_sharing/model/handoff_manager_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "url/gurl.h"

namespace {

// Checks that Handoff will report the specified `gurl`.
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

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

#pragma mark - Tests

// Tests that an empty new tab page should result in no Handoff URL.
- (void)testNewTabPageEmptyURL {
  AssertHandoffURL(GURL());
}

// Tests that the simple case of Handoff URL for a single page.
- (void)testTypicalURL {
  const GURL destinationUrl = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey loadURL:destinationUrl];
  AssertHandoffURL(destinationUrl);
}

// Tests Handoff URL for a new tab.
- (void)testTypicalURLInNewTab {
  [ChromeEarlGrey openNewTab];
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];
  AssertHandoffURL(destinationUrl);
}

// Tests that Handoff URL should never be set for an incognito tab.
- (void)testTypicalURLInNewIncognitoTab {
  // Opens an incognito tab and loads a web page. Check that Handoff URL is nil.
  [ChromeEarlGrey openNewIncognitoTab];
  const GURL destinationUrl = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey loadURL:destinationUrl];
  AssertHandoffURL(GURL());

  // Loads a second URL on the same incognito tab. Handoff URL should still be
  // nil.
  const GURL destinationUrl2 = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl2];
  AssertHandoffURL(GURL());
}

// Tests the state for Handoff URL when creating, closing tab, and switching
// tab.
- (void)testMultipleSwitchingTabs {
  const GURL tab1URL = self.testServer->GetURL("/destination.html");
  const GURL tab2URL = self.testServer->GetURL("/pony.html");
  const GURL tab3URL = self.testServer->GetURL("/chromium_logo_page.html");

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
  const GURL tab1URL = self.testServer->GetURL("/destination.html");
  const GURL tab2URL = self.testServer->GetURL("/pony.html");
  const GURL tab3URL = self.testServer->GetURL("/chromium_logo_page.html");

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
