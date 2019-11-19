// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/content_settings/core/common/content_settings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/net/url_test_util.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::GetCurrentWebState;
using chrome_test_util::OmniboxText;
using chrome_test_util::WebViewMatcher;

using web::test::HttpServer;

namespace {
// URL of the file-based page supporting these tests.
const char kTestURL[] =
    "http://ios/testing/data/http_server_files/window_open.html";

// Returns matcher for Blocked Popup infobar.
id<GREYMatcher> PopupBlocker() {
  NSString* blockerText = base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
      IDS_IOS_POPUPS_BLOCKED_MOBILE, base::UTF8ToUTF16("1")));
  return grey_accessibilityLabel(blockerText);
}

}  // namespace

// Test case for opening child windows by DOM.
@interface WindowOpenByDOMTestCase : ChromeTestCase
@end

@implementation WindowOpenByDOMTestCase

+ (void)setUp {
  [super setUp];
  [ChromeEarlGrey setContentSettings:CONTENT_SETTING_ALLOW];
  web::test::SetUpFileBasedHttpServer();
}

+ (void)tearDown {
  [ChromeEarlGrey setContentSettings:CONTENT_SETTING_DEFAULT];
  [super tearDown];
}

- (void)setUp {
  [super setUp];
  // Open the test page. There should only be one tab open.
  [ChromeEarlGrey loadURL:HttpServer::MakeUrl(kTestURL)];
  [ChromeEarlGrey waitForWebStateContainingText:"Expected result"];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that opening a link with target=_blank which then immediately closes
// itself works.
- (void)testLinkWithBlankTargetWithImmediateClose {
  [ChromeEarlGrey tapWebStateElementWithID:
                      @"webScenarioWindowOpenBlankTargetWithImmediateClose"];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that sessionStorage content is available for windows opened by DOM via
// target="_blank" links.
- (void)testLinkWithBlankTargetSessionStorage {
  [ChromeEarlGrey executeJavaScript:@"sessionStorage.setItem('key', 'value');"];
  const char ID[] = "webScenarioWindowOpenSameURLWithBlankTarget";
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:web::WebViewTapElement(
                        GetCurrentWebState(),
                        [ElementSelector selectorWithElementID:ID])];

  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey waitForWebStateContainingText:"Expected result"];

  id value =
      [ChromeEarlGrey executeJavaScript:@"sessionStorage.getItem('key');"];
  GREYAssert([value isEqual:@"value"], @"sessionStorage is not shared");
}

// Tests tapping a link with target="_blank".
- (void)testLinkWithBlankTarget {
  const char ID[] = "webScenarioWindowOpenRegularLink";
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:web::WebViewTapElement(
                        GetCurrentWebState(),
                        [ElementSelector selectorWithElementID:ID])];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests opening a window with URL that ends with /..;
- (void)testWindowOpenWithSpecialURL {
  const char ID[] = "webScenarioWindowOpenWithSpecialURL";
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:web::WebViewTapElement(
                        GetCurrentWebState(),
                        [ElementSelector selectorWithElementID:ID])];
  if (@available(iOS 13, *)) {
    // Starting from iOS 13 WebKit does not rewrite URL that ends with /..;
    [ChromeEarlGrey waitForMainTabCount:2];
  } else {
    // Prior to iOS 13 WebKit rewries URL that ends with /..; to invalid URL
    // so Chrome opens about:blank for that invalid URL.
    [ChromeEarlGrey waitForMainTabCount:2];
    [[EarlGrey selectElementWithMatcher:OmniboxText("about:blank")]
        assertWithMatcher:grey_notNil()];
  }
}

// Tests executing script that clicks a link with target="_blank".
- (void)testLinkWithBlankTargetWithoutUserGesture {
  [ChromeEarlGrey setContentSettings:CONTENT_SETTING_BLOCK];
  [ChromeEarlGrey
      executeJavaScript:@"document.getElementById('"
                        @"webScenarioWindowOpenRegularLink').click()"];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:PopupBlocker()];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests a link with target="_blank" multiple times.
- (void)testLinkWithBlankTargetMultipleTimes {
  const char ID[] = "webScenarioWindowOpenRegularLinkMultipleTimes";
  web::WebState* test_page_web_state = GetCurrentWebState();
  id<GREYMatcher> test_page_matcher = WebViewMatcher();
  id<GREYAction> link_tap = web::WebViewTapElement(
      test_page_web_state, [ElementSelector selectorWithElementID:ID]);
  [[EarlGrey selectElementWithMatcher:test_page_matcher]
      performAction:link_tap];
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:3];
  [ChromeEarlGrey selectTabAtIndex:0];
  [[EarlGrey selectElementWithMatcher:test_page_matcher]
      performAction:link_tap];
  [ChromeEarlGrey waitForMainTabCount:4];
}

// Tests a window.open by assigning to window.location.
- (void)testWindowOpenAndAssignToHref {
  [ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenTabWithAssignmentToHref"];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that opening a window and calling window.location.assign works.
- (void)testWindowOpenAndCallLocationAssign {
  // Open a child tab.
  [ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenAndCallLocationAssign"];
  [ChromeEarlGrey waitForMainTabCount:2];

  // Ensure that the resulting tab is updated as expected.
  const GURL targetURL =
      HttpServer::MakeUrl(std::string(kTestURL) + "#assigned");
  const std::string targetOmniboxText =
      net::GetContentAndFragmentForUrl(targetURL);
  [[EarlGrey selectElementWithMatcher:OmniboxText(targetOmniboxText)]
      assertWithMatcher:grey_notNil()];
}

// Tests that opening a window, reading its title, and updating its location
// completes and causes a navigation. (Reduced test case from actual site.)
- (void)testWindowOpenAndSetLocation {
  // Open a child tab.
  [ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenAndSetLocation"];
  [ChromeEarlGrey waitForMainTabCount:2];

  // Ensure that the resulting tab is updated as expected.
  const GURL targetURL =
      HttpServer::MakeUrl(std::string(kTestURL) + "#updated");
  const std::string targetOmniboxText =
      net::GetContentAndFragmentForUrl(targetURL);
  [[EarlGrey selectElementWithMatcher:OmniboxText(targetOmniboxText)]
      assertWithMatcher:grey_notNil()];
}

// Tests a button that invokes window.open() with "_blank" target parameter.
- (void)testWindowOpenWithBlankTarget {
  [ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenWithBlankTarget"];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that opening a window with target=_blank which closes itself after 1
// second delay.
- (void)testLinkWithBlankTargetWithDelayedClose {
  const char ID[] = "webScenarioWindowOpenWithDelayedClose";
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:web::WebViewTapElement(
                        GetCurrentWebState(),
                        [ElementSelector selectorWithElementID:ID])];
  [ChromeEarlGrey waitForMainTabCount:2];
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(1));
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests a window.open used in a <button onClick> element.
- (void)testWindowOpenWithButtonOnClick {
  [ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenWithButtonOnClick"];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests a button that invokes window.open with an empty target parameter.
- (void)testWindowOpenWithEmptyTarget {
  [ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenWithEmptyTarget"];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that the correct URL is displayed for a child window opened with the
// script window.open('', '').location.replace('about:blank#hash').
// This is a regression test for crbug.com/866142.
- (void)testLocationReplaceInWindowOpenWithEmptyTarget {
  [ChromeEarlGrey tapWebStateElementWithID:
                      @"webScenarioLocationReplaceInWindowOpenWithEmptyTarget"];
  [ChromeEarlGrey waitForMainTabCount:2];
  // WebKit doesn't parse 'about:blank#hash' as about:blank with URL fragment.
  // Instead, it percent encodes '#hash' and considers 'blank%23hash' as the
  // resource identifier. Nevertheless, the '#' is significant in triggering the
  // edge case in the bug. TODO(crbug.com/885249): Change back to '#'.
  // Since about scheme URLs are also trimmed to about:blank, check the url
  // directly instead.
  DCHECK_EQ(GURL("about:blank%23hash"),
            chrome_test_util::GetCurrentWebState()->GetLastCommittedURL());
  // And confirm the location bar only shows about:blank.
  [[EarlGrey selectElementWithMatcher:OmniboxText("about:blank")]
      assertWithMatcher:grey_notNil()];
}

// Tests a link with JavaScript in the href.
+ (void)testWindowOpenWithJavaScriptInHref {
  [ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenWithJavaScriptInHref"];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests a window.open by running Meta-Refresh.
- (void)testWindowOpenWithMetaRefresh {
  [ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenWithMetaRefresh"];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that a link with an onclick that opens a tab and calls preventDefault
// opens the tab, but doesn't navigate the main tab.
- (void)testWindowOpenWithPreventDefaultLink {
  // Open a child tab.
  [ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenWithPreventDefaultLink"];
  [ChromeEarlGrey waitForMainTabCount:2];

  // Ensure that the starting tab hasn't navigated.
  [ChromeEarlGrey closeCurrentTab];
  const GURL URL = HttpServer::MakeUrl(kTestURL);
  [[EarlGrey selectElementWithMatcher:OmniboxText(URL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that closing the current window using DOM fails.
- (void)testCloseWindowNotOpenByDOM {
  [ChromeEarlGrey tapWebStateElementWithID:@"webScenarioWindowClose"];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that popup blocking works when a popup is injected into a window before
// its initial load is committed.
- (void)testBlockPopupInjectedIntoOpenedWindow {
  [ChromeEarlGrey setContentSettings:CONTENT_SETTING_BLOCK];
  [ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioOpenWindowAndInjectPopup"];
  [[EarlGrey selectElementWithMatcher:PopupBlocker()]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:2];
}

@end
