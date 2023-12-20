// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/scoped_block_popups_pref.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "url/url_constants.h"

namespace {

// Timeout to use when waiting for a condition to be true.
const CFTimeInterval kConditionTimeout = 4.0;
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

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL testURL =
      self.testServer->GetURL("/browsing_prevent_default_test_page.html");
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
  const GURL& currentURL = [ChromeEarlGrey webStateVisibleURL];
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

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL testURL =
      self.testServer->GetURL("/browsing_prevent_default_test_page.html");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Tap on the test link.
  [ChromeEarlGrey tapWebStateElementWithID:@"overrides-window-open"];

  // Check that the tab navigated to about:blank and no new tabs were opened.
  GREYCondition* condition =
      [GREYCondition conditionWithName:@"Wait for navigation to about:blank"
                                 block:^BOOL {
                                   const GURL& currentURL =
                                       [ChromeEarlGrey webStateVisibleURL];
                                   return currentURL == url::kAboutBlankURL;
                                 }];
  GREYAssert([condition waitWithTimeout:kConditionTimeout],
             @"about:blank not loaded.");
  [ChromeEarlGrey waitForMainTabCount:1];
}

@end
