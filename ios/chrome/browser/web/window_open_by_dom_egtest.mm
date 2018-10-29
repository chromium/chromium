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
#include "ios/chrome/test/app/settings_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ExecuteJavaScript;
using chrome_test_util::GetCurrentWebState;
using chrome_test_util::OmniboxText;
using chrome_test_util::TapWebViewElementWithId;
using web::test::ElementSelector;
using web::test::HttpServer;
using web::WebViewInWebState;

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
  chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_ALLOW);
  web::test::SetUpFileBasedHttpServer();
}

+ (void)tearDown {
  chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_DEFAULT);
  [super tearDown];
}

- (void)setUp {
  [super setUp];
  // Open the test page. There should only be one tab open.
  [ChromeEarlGrey loadURL:HttpServer::MakeUrl(kTestURL)];
  [ChromeEarlGrey waitForWebViewContainingText:"Expected result"];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that opening a link with target=_blank which then immediately closes
// itself works.
- (void)testLinkWithBlankTargetWithImmediateClose {
  GREYAssert(
      TapWebViewElementWithId(
          "webScenarioWindowOpenBlankTargetWithImmediateClose"),
      @"Failed to tap \"webScenarioWindowOpenBlankTargetWithImmediateClose\"");
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that sessionStorage content is available for windows opened by DOM via
// target="_blank" links.
- (void)testLinkWithBlankTargetSessionStorage {
  NSError* error = nil;
  ExecuteJavaScript(@"sessionStorage.setItem('key', 'value');", &error);
  GREYAssert(!error, @"Error during script execution: %@", error);
  const char ID[] = "webScenarioWindowOpenSameURLWithBlankTarget";
  [[EarlGrey selectElementWithMatcher:WebViewInWebState(GetCurrentWebState())]
      performAction:web::WebViewTapElement(
                        GetCurrentWebState(),
                        ElementSelector::ElementSelectorId(ID))];

  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey waitForWebViewContainingText:"Expected result"];

  id value = ExecuteJavaScript(@"sessionStorage.getItem('key');", &error);
  GREYAssert(!error, @"Error during script execution: %@", error);
  GREYAssert([value isEqual:@"value"], @"sessionStorage is not shared");
}

// Tests tapping a link with target="_blank".
- (void)testLinkWithBlankTarget {
  const char ID[] = "webScenarioWindowOpenRegularLink";
  [[EarlGrey selectElementWithMatcher:WebViewInWebState(GetCurrentWebState())]
      performAction:web::WebViewTapElement(
                        GetCurrentWebState(),
                        ElementSelector::ElementSelectorId(ID))];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests executing script that clicks a link with target="_blank".
- (void)testLinkWithBlankTargetWithoutUserGesture {
  chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_BLOCK);
  NSError* error = nil;
  ExecuteJavaScript(
      @"document.getElementById('webScenarioWindowOpenRegularLink').click()",
      &error);
  GREYAssert(!error, @"Failed to tap 'webScenarioWindowOpenRegularLink'");
  [ChromeEarlGrey waitForElementWithMatcherSufficientlyVisible:PopupBlocker()];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests a link with target="_blank" multiple times.
- (void)testLinkWithBlankTargetMultipleTimes {
  const char ID[] = "webScenarioWindowOpenRegularLinkMultipleTimes";
  web::WebState* test_page_web_state = GetCurrentWebState();
  id<GREYMatcher> test_page_matcher = WebViewInWebState(test_page_web_state);
  id<GREYAction> link_tap = web::WebViewTapElement(
      test_page_web_state, ElementSelector::ElementSelectorId(ID));
  [[EarlGrey selectElementWithMatcher:test_page_matcher]
      performAction:link_tap];
  [ChromeEarlGrey waitForMainTabCount:2];
  chrome_test_util::OpenNewTab();
  [ChromeEarlGrey waitForMainTabCount:3];
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);
  [[EarlGrey selectElementWithMatcher:test_page_matcher]
      performAction:link_tap];
  [ChromeEarlGrey waitForMainTabCount:4];
}

// Tests a window.open by assigning to window.location.
- (void)testWindowOpenAndAssignToHref {
  GREYAssert(
      TapWebViewElementWithId("webScenarioWindowOpenTabWithAssignmentToHref"),
      @"Failed to tap \"webScenarioWindowOpenTabWithAssignmentToHref\"");
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that opening a window and calling window.location.assign works.
- (void)testWindowOpenAndCallLocationAssign {
  // Open a child tab.
  GREYAssert(
      TapWebViewElementWithId("webScenarioWindowOpenAndCallLocationAssign"),
      @"Failed to tap \"webScenarioWindowOpenAndCallLocationAssign\"");
  [ChromeEarlGrey waitForMainTabCount:2];

  // Ensure that the resulting tab is updated as expected.
  const GURL targetURL =
      HttpServer::MakeUrl(std::string(kTestURL) + "#assigned");
  [[EarlGrey selectElementWithMatcher:OmniboxText(targetURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that opening a window, reading its title, and updating its location
// completes and causes a navigation. (Reduced test case from actual site.)
- (void)testWindowOpenAndSetLocation {
  // Open a child tab.
  GREYAssert(TapWebViewElementWithId("webScenarioWindowOpenAndSetLocation"),
             @"Failed to tap \"webScenarioWindowOpenAndSetLocation\"");
  [ChromeEarlGrey waitForMainTabCount:2];

  // Ensure that the resulting tab is updated as expected.
  const GURL targetURL =
      HttpServer::MakeUrl(std::string(kTestURL) + "#updated");
  [[EarlGrey selectElementWithMatcher:OmniboxText(targetURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests a button that invokes window.open() with "_blank" target parameter.
- (void)testWindowOpenWithBlankTarget {
  GREYAssert(TapWebViewElementWithId("webScenarioWindowOpenWithBlankTarget"),
             @"Failed to tap \"webScenarioWindowOpenWithBlankTarget\"");
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that opening a window with target=_blank which closes itself after 1
// second delay.
- (void)testLinkWithBlankTargetWithDelayedClose {
  const char ID[] = "webScenarioWindowOpenWithDelayedClose";
  [[EarlGrey selectElementWithMatcher:WebViewInWebState(GetCurrentWebState())]
      performAction:web::WebViewTapElement(
                        GetCurrentWebState(),
                        ElementSelector::ElementSelectorId(ID))];
  [ChromeEarlGrey waitForMainTabCount:2];
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(1));
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests a window.open used in a <button onClick> element.
- (void)testWindowOpenWithButtonOnClick {
  GREYAssert(TapWebViewElementWithId("webScenarioWindowOpenWithButtonOnClick"),
             @"Failed to tap \"webScenarioWindowOpenWithButtonOnClick\"");
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests a button that invokes window.open with an empty target parameter.
- (void)testWindowOpenWithEmptyTarget {
  GREYAssert(TapWebViewElementWithId("webScenarioWindowOpenWithEmptyTarget"),
             @"Failed to tap \"webScenarioWindowOpenWithEmptyTarget\"");
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that the correct URL is displayed for a child window opened with the
// script window.open('', '').location.replace('about:blank#hash').
// This is a regression test for crbug.com/866142.
- (void)testLocationReplaceInWindowOpenWithEmptyTarget {
  GREYAssert(TapWebViewElementWithId(
                 "webScenarioLocationReplaceInWindowOpenWithEmptyTarget"),
             @"Failed to tap "
             @"\"webScenarioLocationReplaceInWindowOpenWithEmptyTarget\"");
  [ChromeEarlGrey waitForMainTabCount:2];
  // WebKit doesn't parse 'about:blank#hash' as about:blank with URL fragment.
  // Instead, it percent encodes '#hash' and considers 'blank%23hash' as the
  // resource identifier. Nevertheless, the '#' is significant in triggering the
  // edge case in the bug. TODO(crbug.com/885249): Change back to '#'.
  const GURL URL("about:blank%23hash");
  [[EarlGrey selectElementWithMatcher:OmniboxText("about:blank%23hash")]
      assertWithMatcher:grey_notNil()];
}

// Tests a link with JavaScript in the href.
+ (void)testWindowOpenWithJavaScriptInHref {
  GREYAssert(
      TapWebViewElementWithId("webScenarioWindowOpenWithJavaScriptInHref"),
      @"Failed to tap \"webScenarioWindowOpenWithJavaScriptInHref\"");
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests a window.open by running Meta-Refresh.
- (void)testWindowOpenWithMetaRefresh {
  GREYAssert(TapWebViewElementWithId("webScenarioWindowOpenWithMetaRefresh"),
             @"Failed to tap \"webScenarioWindowOpenWithMetaRefresh\"");
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that a link with an onclick that opens a tab and calls preventDefault
// opens the tab, but doesn't navigate the main tab.
- (void)testWindowOpenWithPreventDefaultLink {
  // Open a child tab.
  GREYAssert(
      TapWebViewElementWithId("webScenarioWindowOpenWithPreventDefaultLink"),
      @"Failed to tap \"webScenarioWindowOpenWithPreventDefaultLink\"");
  [ChromeEarlGrey waitForMainTabCount:2];

  // Ensure that the starting tab hasn't navigated.
  chrome_test_util::CloseCurrentTab();
  const GURL URL = HttpServer::MakeUrl(kTestURL);
  [[EarlGrey selectElementWithMatcher:OmniboxText(URL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that closing the current window using DOM fails.
- (void)testCloseWindowNotOpenByDOM {
  GREYAssert(TapWebViewElementWithId("webScenarioWindowClose"),
             @"Failed to tap \"webScenarioWindowClose\"");
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that popup blocking works when a popup is injected into a window before
// its initial load is committed.
- (void)testBlockPopupInjectedIntoOpenedWindow {
  chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_BLOCK);
  GREYAssert(TapWebViewElementWithId("webScenarioOpenWindowAndInjectPopup"),
             @"Failed to tap \"webScenarioOpenWindowAndInjectPopup\"");
  [[EarlGrey selectElementWithMatcher:PopupBlocker()]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:2];
}

@end
