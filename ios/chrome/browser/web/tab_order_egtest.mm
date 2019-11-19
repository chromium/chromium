// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::OpenLinkInNewTabButton;
using chrome_test_util::WebViewMatcher;

namespace {

// This test uses test pages from in //ios/testing/data/http_server_files/.

// URL for a test page that contains a link.
const char kLinksTestURL1[] = "/links.html";

// Some text in |kLinksTestURL1|.
const char kLinksTestURL1Text[] = "Normal Link";

// ID of the <a> link in |kLinksTestURL1|.
const char kLinkSelectorID[] = "normal-link";

// URL for a different test page.
const char kLinksTestURL2[] = "/destination.html";

// Some text in |kLinksTestURL2|.
const char kLinksTestURL2Text[] = "arrived";

}  // namespace

// Tests the order in which new tabs are created.
@interface TabOrderTestCase : ChromeTestCase
@end

@implementation TabOrderTestCase

// Tests that new tabs are always inserted after their parent tab.
- (void)testChildTabOrdering {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL1 = self.testServer->GetURL(kLinksTestURL1);

  // Create a tab that will act as the parent tab.
  [ChromeEarlGrey loadURL:URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kLinksTestURL1Text];
  NSString* parentTabID = [ChromeEarlGrey currentTabID];
  // Child tab should be inserted after the parent.
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:kLinkSelectorID],
                        true /* menu should appear */)];
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForMainTabCount:2U];
  NSString* childTab1ID = [ChromeEarlGrey nextTabID];

  // New child tab should be inserted after the tab with |childTab1ID|.
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:kLinkSelectorID],
                        true /* menu should appear */)];
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForMainTabCount:3U];
  GREYAssertEqualObjects(childTab1ID, [ChromeEarlGrey nextTabID],
                         @"Unexpected next tab");

  // Navigate the parent tab away and again to |kLinksTestURL1| to break
  // grouping with the current child tabs. Total number of tabs should not
  // change.
  const GURL URL2 = self.testServer->GetURL(kLinksTestURL2);
  [ChromeEarlGrey loadURL:URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kLinksTestURL2Text];
  GREYAssertEqual(3U, [ChromeEarlGrey mainTabCount],
                  @"Unexpected number of tabs");

  [ChromeEarlGrey loadURL:URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kLinksTestURL1Text];
  GREYAssertEqual(3U, [ChromeEarlGrey mainTabCount],
                  @"Unexpected number of tabs");

  // New child tab should be inserted before the tab with |childTab1ID|.
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:kLinkSelectorID],
                        true /* menu should appear */)];
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForMainTabCount:4U];
  NSString* childTab3ID = [ChromeEarlGrey nextTabID];

  GREYAssertNotEqualObjects(childTab1ID, childTab3ID, @"Unexpected next tab");

  // New child tab should be inserted after the tab with |childTab3ID|.
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:kLinkSelectorID],
                        true /* menu should appear */)];
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForMainTabCount:5U];
  GREYAssertEqualObjects(childTab3ID, [ChromeEarlGrey nextTabID],
                         @"Unexpected next web state");

  // Verify that tab with |childTab1ID| is now at index 3.
  [ChromeEarlGrey selectTabAtIndex:3];
  GREYAssertEqualObjects(childTab1ID, [ChromeEarlGrey currentTabID],
                         @"Unexpected current web state");

  // Add a non-owned tab. It should be added at the end and marked as the
  // current web state. Next web state should wrap back to index 0, the original
  // parent web state.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:6U];
  GREYAssertEqualObjects(parentTabID, [ChromeEarlGrey nextTabID],
                         @"Unexpected next web state");

  // Verify that tab with |anotherTabID| is at index 5.
  NSString* anotherTabID = [ChromeEarlGrey currentTabID];
  [ChromeEarlGrey selectTabAtIndex:5];
  GREYAssertEqualObjects(anotherTabID, [ChromeEarlGrey currentTabID],
                         @"Unexpected current web state");
}

@end
