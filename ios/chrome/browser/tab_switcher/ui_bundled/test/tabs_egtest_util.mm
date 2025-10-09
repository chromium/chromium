// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/test/tabs_egtest_util.h"

#import "base/ios/block_types.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/popup_menu_constants.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/test/query_title_server_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

namespace {

NSString* const kRegularTabTitlePrefix = @"RegularTab";
NSString* const kPinnedTabTitlePrefix = @"PinnedTab";

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
    [ChromeEarlGrey pinCurrentTab];
  }
}

id<GREYMatcher> TabGridCell() {
  return grey_allOf(grey_kindOfClassName(@"GridCell"),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TabWithTitle(NSString* title) {
  return grey_allOf(TabGridCell(), grey_accessibilityLabel(title),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TabWithTitleAndIndex(NSString* title, unsigned int index) {
  return grey_allOf(TabWithTitle(title),
                    chrome_test_util::TabGridCellAtIndex(index), nil);
}

void WaitForSnackbarTriggeredByTappingItem(NSString* snackbarLabel,
                                           id<GREYMatcher> matcher) {
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];

  // Wait for the snackbar to appear.
  id<GREYMatcher> snackbar_matcher = chrome_test_util::SnackbarViewMatcher();
  [ChromeEarlGrey testUIElementAppearanceWithMatcher:snackbar_matcher];
  // Tap the snackbar to make it disappear.
  [[EarlGrey selectElementWithMatcher:snackbar_matcher]
      performAction:grey_tap()];
}
