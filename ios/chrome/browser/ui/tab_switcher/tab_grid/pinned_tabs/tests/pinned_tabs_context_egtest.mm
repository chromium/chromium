// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/test/query_title_server_util.h"
#import "ios/chrome/browser/ui/tab_switcher/test/tabs_egtest_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using chrome_test_util::ContextMenuItemWithAccessibilityLabelId;

namespace {

// Matcher for the regual cell at the given `index`.
id<GREYMatcher> GetMatcherForRegularCellWithTitle(NSString* title) {
  return grey_allOf(grey_accessibilityLabel(title),
                    grey_ancestor(grey_kindOfClassName(@"GridCell")),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the pinned cell at the given `index`.
id<GREYMatcher> GetMatcherForPinnedCellWithTitle(NSString* title) {
  return grey_allOf(grey_accessibilityLabel(title),
                    grey_ancestor(grey_kindOfClassName(@"PinnedCell")),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the pinned view.
id<GREYMatcher> GetMatcherForPinnedView() {
  return grey_allOf(grey_accessibilityID(@"PinnedViewIdentifier"),
                    grey_sufficientlyVisible(), nil);
}

// Checks that the regular cell with `tab_title` has been moved to the pinned
// view.
void AssertRegularCellMovedToPinnedView(NSString* tab_title) {
  ConditionBlock condition = ^{
    NSError* error1 = nil;
    NSError* error2 = nil;

    [[EarlGrey
        selectElementWithMatcher:GetMatcherForRegularCellWithTitle(tab_title)]
        assertWithMatcher:grey_nil()
                    error:&error1];
    [[EarlGrey
        selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(tab_title)]
        assertWithMatcher:grey_notNil()
                    error:&error2];

    return !error1 && !error2;
  };

  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Has failed to pin a tab with title: \"%@.\"", tab_title);
}

// Checks that the pinned cell with `tab_title` has been moved to the tab grid
// view.
void AssertPinnedCellMovedToGridView(NSString* tab_title) {
  ConditionBlock condition = ^{
    NSError* error1 = nil;
    NSError* error2 = nil;

    [[EarlGrey
        selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(tab_title)]
        assertWithMatcher:grey_nil()
                    error:&error1];
    [[EarlGrey
        selectElementWithMatcher:GetMatcherForRegularCellWithTitle(tab_title)]
        assertWithMatcher:grey_notNil()
                    error:&error2];

    return !error1 && !error2;
  };

  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Has failed to unpin a tab with title: \"%@.\"", tab_title);
}

}  // namespace

// Tests related to Pinned Tabs feature on the OverflowMenu surface.
@interface PinnedTabsContextMenuTestCase : ChromeTestCase
@end

@implementation PinnedTabsContextMenuTestCase

// Sets up the EmbeddedTestServer as needed for tests.
- (void)setUpTestServer {
  RegisterQueryTitleHandler(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");
}

- (void)setUp {
  [super setUp];

  [self setUpTestServer];
}

// Pins a regular tab using the context menu (other pinned tabs are NOT
// present).
- (void)testPinFirstTabFromContextMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }

  // Create tabs.
  CreateRegularTabs(1, self.testServer);

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // The pinned view should not be visible when there is no pinned tabs.
  [[EarlGrey selectElementWithMatcher:GetMatcherForPinnedView()]
      assertWithMatcher:grey_notVisible()];

  // Long tap on the first regular tab.
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab0")]
      performAction:grey_longPress()];

  // Tap on "Pin Tab" context menu action.
  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                          IDS_IOS_CONTENT_CONTEXT_PINTAB)]
      performAction:grey_tap()];

  // Check that regular tab was pinned.
  AssertRegularCellMovedToPinnedView(@"RegularTab0");

  // Check the pinned view is visible when there is a pinned tab in it.
  [[EarlGrey selectElementWithMatcher:GetMatcherForPinnedView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Pins a regular tab using the context menu (other pinned tabs are present).
- (void)testPinTabFromContextMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }

  // Create tabs.
  CreatePinnedTabs(1, self.testServer);
  CreateRegularTabs(1, self.testServer);

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // The pinned view should be visible when there are pinned tabs created.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:GetMatcherForPinnedView()];

  // Long tap on the first regular tab.
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab0")]
      performAction:grey_longPress()];

  // Tap on "Pin Tab" context menu action.
  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                          IDS_IOS_CONTENT_CONTEXT_PINTAB)]
      performAction:grey_tap()];

  // Check that regular tab was pinned.
  AssertRegularCellMovedToPinnedView(@"RegularTab0");
}

// Unpins a pinned tab using the context menu (other pinned tabs are present).
- (void)testUnpinTabFromContextMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }

  // Create tabs.
  CreatePinnedTabs(2, self.testServer);
  CreateRegularTabs(1, self.testServer);

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // The pinned view should be visible when there are pinned tabs created.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:GetMatcherForPinnedView()];

  // Long tap on the first pinned tab.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab0")]
      performAction:grey_longPress()];

  // Tap on "Unpin Tab" context menu action.
  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                          IDS_IOS_CONTENT_CONTEXT_UNPINTAB)]
      performAction:grey_tap()];

  // Check that the pinned tab was unpinned.
  AssertPinnedCellMovedToGridView(@"PinnedTab0");
}

// Unpins last pinned tab using the context menu.
- (void)testUnpinLastTabFromContextMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }

  // Create tabs.
  CreatePinnedTabs(1, self.testServer);
  CreateRegularTabs(1, self.testServer);

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // The pinned view should be visible when there are pinned tabs created.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:GetMatcherForPinnedView()];

  // Long tap on the first pinned tab.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab0")]
      performAction:grey_longPress()];

  // Tap on "Unpin Tab" context menu action.
  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                          IDS_IOS_CONTENT_CONTEXT_UNPINTAB)]
      performAction:grey_tap()];

  // Check that the pinned tab was unpinned.
  AssertPinnedCellMovedToGridView(@"PinnedTab0");
}

@end
