// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/chrome/test/earl_grey/scoped_block_popups_pref.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "ios/web/public/web_state.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::GetOriginalBrowserState;

namespace {

// Timeout to use when waiting for a condition to be true.
const CFTimeInterval kConditionTimeout = 4.0;

// Returns the URL for the HTML that is used for testing purposes in this file.
GURL GetTestUrl() {
  return web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/"
      "browsing_prevent_default_test_page.html");
}
}  // namespace

// Tests that the javascript preventDefault() function correctly prevents new
// tabs from opening or navigation from occurring.
@interface BrowsingPreventDefaultTestCase : ChromeTestCase
@end

@implementation BrowsingPreventDefaultTestCase

// Helper function to tap a link and verify that the URL did not change and no
// new tabs were opened.
- (void)runTestAndVerifyNoNavigationForLinkID:(const std::string&)linkID {
  // Disable popup blocking, because that will mask failures that try to open
  // new tabs.
  ScopedBlockPopupsPref scoper(CONTENT_SETTING_ALLOW);
  web::test::SetUpFileBasedHttpServer();

  const GURL testURL = GetTestUrl();
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Tap on the test link and wait for the page to display "Click done", as an
  // indicator that the element was tapped.
  [ChromeEarlGrey
      tapWebStateElementWithID:
          [NSString stringWithCString:linkID.c_str()
                             encoding:[NSString defaultCStringEncoding]]];
  [ChromeEarlGrey waitForWebStateContainingText:"Click done"];

  // Check that no navigation occurred and no new tabs were opened.
  [ChromeEarlGrey waitForMainTabCount:1];
  const GURL& currentURL =
      chrome_test_util::GetCurrentWebState()->GetVisibleURL();
  GREYAssert(currentURL == testURL, @"Page navigated unexpectedly %s",
             currentURL.spec().c_str());
}

// Taps a link with onclick="event.preventDefault()" and target="_blank" and
// verifies that the URL didn't change and no tabs were opened.
- (void)testPreventDefaultOverridesTargetBlank {
  [self runTestAndVerifyNoNavigationForLinkID:"overrides-target-blank"];
}

// Tests clicking a link with target="_blank" and event 'preventDefault()' and
// 'stopPropagation()' does not change the current URL nor open a new tab.
- (void)testPreventDefaultOverridesStopPropagation {
  [self runTestAndVerifyNoNavigationForLinkID:"overrides-stop-propagation"];
}

// Tests clicking a link with event 'preventDefault()' and URL loaded by
// JavaScript does not open a new tab, but does navigate to the URL.
- (void)testPreventDefaultOverridesWindowOpen {
  // Disable popup blocking, because that will mask failures that try to open
  // new tabs.
  ScopedBlockPopupsPref scoper(CONTENT_SETTING_ALLOW);
  web::test::SetUpFileBasedHttpServer();

  const GURL testURL = GetTestUrl();
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Tap on the test link.
  [ChromeEarlGrey tapWebStateElementWithID:@"overrides-window-open"];

  // Check that the tab navigated to about:blank and no new tabs were opened.
  [[GREYCondition
      conditionWithName:@"Wait for navigation to about:blank"
                  block:^BOOL {
                    const GURL& currentURL =
                        chrome_test_util::GetCurrentWebState()->GetVisibleURL();
                    return currentURL == url::kAboutBlankURL;
                  }] waitWithTimeout:kConditionTimeout];
  [ChromeEarlGrey waitForMainTabCount:1];
}

@end
