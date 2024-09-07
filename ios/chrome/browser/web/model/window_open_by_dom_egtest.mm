// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/format_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/content_settings/core/common/content_settings.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/scoped_block_popups_pref.h"
#import "ios/net/url_test_util.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::OmniboxText;

namespace {
// URL of the file-based page supporting these tests.
const char kTestURL[] = "/window_open.html";

// Returns matcher for Blocked Popup infobar labels.
id<GREYMatcher> PopupBlocker() {
  return grey_allOf(
      grey_accessibilityID(kInfobarBannerLabelsStackViewIdentifier),
      grey_accessibilityLabel(base::SysUTF16ToNSString(
          l10n_util::GetStringFUTF16(IDS_IOS_POPUPS_BLOCKED_MOBILE, u"1"))),
      nil);
}

}  // namespace

// Test case for opening child windows by DOM.
@interface WindowOpenByDOMTestCase : ChromeTestCase {
  std::unique_ptr<ScopedBlockPopupsPref> _blockPopupsPref;
}

@end

@implementation WindowOpenByDOMTestCase

- (void)setUp {
  [super setUp];
  _blockPopupsPref =
      std::make_unique<ScopedBlockPopupsPref>(CONTENT_SETTING_ALLOW);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  // Open the test page. There should only be one tab open.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kTestURL)];
  [ChromeEarlGrey waitForWebStateContainingText:"Expected result"];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that opening a link with target=_blank which then immediately closes
// itself works.
// TODO(crbug.com/361752763): This test started to be flaky on 2024-08-07.
- (void)FLAKY_testLinkWithBlankTargetWithImmediateClose {
  [ChromeEarlGrey tapWebStateElementWithID:
                      @"webScenarioWindowOpenBlankTargetWithImmediateClose"];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests tapping a link with target="_blank".
- (void)testLinkWithBlankTarget {
  [ChromeEarlGrey tapWebStateElementWithID:@"webScenarioWindowOpenRegularLink"];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests opening a window with URL that ends with /..;
- (void)testWindowOpenWithSpecialURL {
  [ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenWithSpecialURL"];
  // Starting from iOS 13 WebKit does not rewrite URL that ends with /..;
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests executing script that clicks a link with target="_blank".
- (void)testLinkWithBlankTargetWithoutUserGesture {
  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_BLOCK);
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:
                      @"document.getElementById('"
                      @"webScenarioWindowOpenRegularLink').click()"];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:PopupBlocker()];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests a link with target="_blank" multiple times.
- (void)testLinkWithBlankTargetMultipleTimes {
  [ChromeEarlGrey tapWebStateElementWithID:
                      @"webScenarioWindowOpenRegularLinkMultipleTimes"];
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:3];
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey tapWebStateElementWithID:
                      @"webScenarioWindowOpenRegularLinkMultipleTimes"];
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
      self.testServer->GetURL(std::string(kTestURL) + "#assigned");
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
      self.testServer->GetURL(std::string(kTestURL) + "#updated");
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
  [ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenWithDelayedClose"];
  [ChromeEarlGrey waitForMainTabCount:2];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(5));
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
  // edge case in the bug. TODO(crbug.com/41414501): Change back to '#'.
  // Since about scheme URLs are also trimmed to about:blank, check the url
  // directly instead.
  //
  // TODO(crbug.com/40932726): Confirm the expected behavir of [ChromeEarlGrey
  // webStateLastCommittedURL] here. After https://crrev.com/c/4823237, this
  // returns empty URL ("").
  DCHECK_EQ("",
            [ChromeEarlGrey webStateLastCommittedURL]);
  // And confirm the location bar only shows "".
  [[EarlGrey selectElementWithMatcher:OmniboxText("")]
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
  const GURL URL = self.testServer->GetURL(kTestURL);
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
  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_BLOCK);
  [ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioOpenWindowAndInjectPopup"];
  [[EarlGrey selectElementWithMatcher:PopupBlocker()]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:2];
}

@end
