// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/test/tabs_egtest_util.h"

#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/test/query_title_server_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kRegularTabTitlePrefix = @"RegularTab";
NSString* const kPinnedTabTitlePrefix = @"PinnedTab";

// Matcher for the overflow pin action.
id<GREYMatcher> GetMatcherForPinOverflowAction() {
  return grey_accessibilityID(kToolsMenuPinTabId);
}

// Pins a regular tab using overflow menu.
void PinTabUsingOverflowMenu() {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuAction:GetMatcherForPinOverflowAction()];
}

}  // namespace

// Creates a regular tab with `title` using `test_server`.
void CreateRegularTab(net::test_server::EmbeddedTestServer* test_server,
                      NSString* title) {
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:GetQueryTitleURL(test_server, title)];
}

// Create `tabs_count` of regular tabs.
void CreateRegularTabs(int tabs_count,
                       net::test_server::EmbeddedTestServer* test_server) {
  for (int index = 0; index < tabs_count; ++index) {
    NSString* title =
        [kRegularTabTitlePrefix stringByAppendingFormat:@"%d", index];

    CreateRegularTab(test_server, title);
  }
}

// Create `tabs_count` of pinned tabs.
void CreatePinnedTabs(int tabs_count,
                      net::test_server::EmbeddedTestServer* test_server) {
  for (int index = 0; index < tabs_count; ++index) {
    NSString* title =
        [kPinnedTabTitlePrefix stringByAppendingFormat:@"%d", index];

    CreateRegularTab(test_server, title);
    PinTabUsingOverflowMenu();
  }
}
