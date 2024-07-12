// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
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

NSString* const kRegularTabTitlePrefix = @"RegularTab";
NSString* const kPinnedTabTitlePrefix = @"PinnedTab";

// net::EmbeddedTestServer handler that responds with the request's query as the
// title and body.
std::unique_ptr<net::test_server::HttpResponse> HandleQueryTitle(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_content_type("text/html");
  http_response->set_content("<html><head><title>" + request.GetURL().query() +
                             "</title></head><body>" +
                             request.GetURL().query() + "</body></html>");
  return std::move(http_response);
}

// Matcher for the regual cell at the given `index`.
id<GREYMatcher> GetMatcherForRegularCellWithTitle(NSString* title) {
  return grey_allOf(grey_accessibilityLabel(title),
                    grey_kindOfClassName(@"GridCell"),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the pinned cell at the given `index`.
id<GREYMatcher> GetMatcherForPinnedCellWithTitle(NSString* title) {
  return grey_allOf(
      grey_accessibilityLabel([NSString stringWithFormat:@"Pinned, %@", title]),
      grey_kindOfClassName(@"PinnedCell"), grey_sufficientlyVisible(), nil);
}

// Matcher for the "Done" button on the Tab Grid.
id<GREYMatcher> GetMatcherForDoneButton() {
  return grey_accessibilityID(kTabGridDoneButtonIdentifier);
}

// Matcher for the "Edit" button on the Tab Grid.
id<GREYMatcher> GetMatcherForEditButton() {
  return grey_accessibilityID(kTabGridEditButtonIdentifier);
}

// Matcher for the "Undo" button on the Tab Grid.
id<GREYMatcher> GetMatcherForUndoButton() {
  return grey_accessibilityID(kTabGridUndoCloseAllButtonIdentifier);
}

// Matcher for the pinned view.
id<GREYMatcher> GetMatcherForPinnedView() {
  return grey_allOf(grey_accessibilityID(kPinnedViewIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Returns the URL for a test page with the given `title`.
GURL GetURLForTitle(net::EmbeddedTestServer* test_server, NSString* title) {
  return test_server->GetURL("/querytitle?" + base::SysNSStringToUTF8(title));
}

}  // namespace

// Tests related to Pinned Tabs feature on the OverflowMenu surface.
@interface PinnedTabsGenericConsistencyTestCase : ChromeTestCase
@end

@implementation PinnedTabsGenericConsistencyTestCase

// Waits for the animation (context modal disappearance) to complete.
- (void)waitForAnimationCompletionWithMacther:(id<GREYMatcher>)elementMatcher {
  // Wait until it's gone.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:elementMatcher]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Has failed to find an element after animation completion: %@",
             elementMatcher);
}

// Sets up the EmbeddedTestServer as needed for tests.
- (void)setUpTestServer {
  self.testServer->RegisterDefaultHandler(base::BindRepeating(
      net::test_server::HandlePrefixedRequest, "/querytitle",
      base::BindRepeating(&HandleQueryTitle)));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");
}

- (void)setUp {
  [super setUp];

  [self setUpTestServer];
}

// Tests that there is only one active (selected) tab at a time.
- (void)testOneActiveTabAtATime {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }

  // Create tabs.
  CreatePinnedTabs(2, self.testServer);
  CreateRegularTabs(2, self.testServer);

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Check there is only one selected item.
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab0")]
      assertWithMatcher:grey_not(grey_selected())];
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab1")]
      assertWithMatcher:grey_selected()];
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab0")]
      assertWithMatcher:grey_not(grey_selected())];
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab1")]
      assertWithMatcher:grey_not(grey_selected())];

  // Tap on the first pinned tab.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab0")]
      performAction:grey_tap()];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Check there is only one selected item.
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab0")]
      assertWithMatcher:grey_not(grey_selected())];
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab1")]
      assertWithMatcher:grey_not(grey_selected())];
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab0")]
      assertWithMatcher:grey_selected()];
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab1")]
      assertWithMatcher:grey_not(grey_selected())];

  // Tap on the second pinned tab.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab1")]
      performAction:grey_tap()];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Check there is only one selected item.
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab0")]
      assertWithMatcher:grey_not(grey_selected())];
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab1")]
      assertWithMatcher:grey_not(grey_selected())];
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab0")]
      assertWithMatcher:grey_not(grey_selected())];
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab1")]
      assertWithMatcher:grey_selected()];

  // Tap on the second regular tab.
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab1")]
      performAction:grey_tap()];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Check there is only one selected item.
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab0")]
      assertWithMatcher:grey_not(grey_selected())];
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab1")]
      assertWithMatcher:grey_selected()];
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab0")]
      assertWithMatcher:grey_not(grey_selected())];
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab1")]
      assertWithMatcher:grey_not(grey_selected())];
}

// Tests that active tabs are open by tapping the "Done" button.
- (void)testTabIsOpenByTappingDoneButton {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }

  // Create tabs.
  CreatePinnedTabs(1, self.testServer);
  CreateRegularTabs(1, self.testServer);

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Tap "Done" button
  [[EarlGrey selectElementWithMatcher:GetMatcherForDoneButton()]
      performAction:grey_tap()];

  // Check that the opened tab has a correct title.
  GREYAssert([[ChromeEarlGrey currentTabTitle] isEqualToString:@"RegularTab0"],
             @"The tab title is not correct!");

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Tap on the pinned tab.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab0")]
      performAction:grey_tap()];

  // Check that the opened tab has a correct title.
  GREYAssert([[ChromeEarlGrey currentTabTitle] isEqualToString:@"PinnedTab0"],
             @"The tab title is not correct!");

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Tap "Done" button.
  [[EarlGrey selectElementWithMatcher:GetMatcherForDoneButton()]
      performAction:grey_tap()];

  // Check that the opened tab has a correct title.
  GREYAssert([[ChromeEarlGrey currentTabTitle] isEqualToString:@"PinnedTab0"],
             @"The tab title is not correct!");
}

// Tests that Pinned Tab updates when navigated to another site.
- (void)testPinnedTabUpdatesWhenNavigatedToAnotherSite {
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

  // Verify the pinned tab has a correct title.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab0")]
      assertWithMatcher:grey_notNil()];

  // Tap on the pinned tab.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab0")]
      performAction:grey_tap()];

  // Load another URL.
  [ChromeEarlGrey loadURL:GetURLForTitle(self.testServer, @"SomeOtherWebSite")];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Verify the pinned tab has a correct title.
  [[EarlGrey selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(
                                          @"SomeOtherWebSite")]
      assertWithMatcher:grey_notNil()];
}

// Tests closing all the regular tabs and then all the pinned tabs.
- (void)testCloseAllRegularThenPinnedTabs {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }

  // Create tabs.
  CreatePinnedTabs(2, self.testServer);
  CreateRegularTabs(1, self.testServer);

  // Close NTP tab.
  [ChromeEarlGrey closeTabAtIndex:2];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Verify "Done" button is enabled.
  [[EarlGrey selectElementWithMatcher:GetMatcherForDoneButton()]
      assertWithMatcher:grey_enabled()];

  // Verify "Edit" button is enabled.
  [[EarlGrey selectElementWithMatcher:GetMatcherForEditButton()]
      assertWithMatcher:grey_enabled()];

  // Long tap on the first regular tab.
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab0")]
      performAction:grey_longPress()];

  // Tap on "Close Tab" context menu action.
  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                          IDS_IOS_CONTENT_CONTEXT_CLOSETAB)]
      performAction:grey_tap()];

  // Verify "Done" button is enabled.
  [[EarlGrey selectElementWithMatcher:GetMatcherForDoneButton()]
      assertWithMatcher:grey_enabled()];

  // Verify "Edit" button is disabled.
  [[EarlGrey selectElementWithMatcher:GetMatcherForEditButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  [self waitForAnimationCompletionWithMacther:GetMatcherForPinnedCellWithTitle(
                                                  @"PinnedTab0")];

  // Long tap on the first pinned tab.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab0")]
      performAction:grey_longPress()];

  // Tap on "Close Pinned Tab" context menu action.
  [[EarlGrey
      selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_CLOSEPINNEDTAB)]
      performAction:grey_tap()];

  [self waitForAnimationCompletionWithMacther:GetMatcherForPinnedCellWithTitle(
                                                  @"PinnedTab1")];

  // Long tap on the other pinned tab.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab1")]
      performAction:grey_longPress()];

  // Tap on "Close Pinned Tab" context menu action.
  [[EarlGrey
      selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_CLOSEPINNEDTAB)]
      performAction:grey_tap()];

  // Verify "Done" button is disabled.
  [[EarlGrey selectElementWithMatcher:GetMatcherForDoneButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Verify "Edit" button is disabled.
  [[EarlGrey selectElementWithMatcher:GetMatcherForEditButton()]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Tests closing all the pinned tabs and then all the regular tabs.
- (void)testCloseAllPinnedThenRegularTabs {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }

  // Create tabs.
  CreatePinnedTabs(2, self.testServer);
  CreateRegularTabs(1, self.testServer);

  // Close NTP tab.
  [ChromeEarlGrey closeTabAtIndex:2];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Verify "Done" button is enabled.
  [[EarlGrey selectElementWithMatcher:GetMatcherForDoneButton()]
      assertWithMatcher:grey_enabled()];

  // Verify "Edit" button is enabled.
  [[EarlGrey selectElementWithMatcher:GetMatcherForEditButton()]
      assertWithMatcher:grey_enabled()];

  // Long tap on the first pinned tab.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab0")]
      performAction:grey_longPress()];

  // Tap on "Close Pinned Tab" context menu action.
  [[EarlGrey
      selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_CLOSEPINNEDTAB)]
      performAction:grey_tap()];

  [self waitForAnimationCompletionWithMacther:GetMatcherForPinnedCellWithTitle(
                                                  @"PinnedTab1")];

  // Long tap on the other pinned tab.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab1")]
      performAction:grey_longPress()];

  // Tap on "Close Pinned Tab" context menu action.
  [[EarlGrey
      selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_CLOSEPINNEDTAB)]
      performAction:grey_tap()];

  // Verify "Done" button is enabled.
  [[EarlGrey selectElementWithMatcher:GetMatcherForDoneButton()]
      assertWithMatcher:grey_enabled()];

  // Verify "Edit" button is enabled.
  [[EarlGrey selectElementWithMatcher:GetMatcherForEditButton()]
      assertWithMatcher:grey_enabled()];

  [self waitForAnimationCompletionWithMacther:GetMatcherForRegularCellWithTitle(
                                                  @"RegularTab0")];

  // Long tap on the first regular tab.
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab0")]
      performAction:grey_longPress()];

  // Tap on "Close Tab" context menu action.
  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                          IDS_IOS_CONTENT_CONTEXT_CLOSETAB)]
      performAction:grey_tap()];

  // Verify "Done" button is disabled.
  [[EarlGrey selectElementWithMatcher:GetMatcherForDoneButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Verify "Edit" button is disabled.
  [[EarlGrey selectElementWithMatcher:GetMatcherForEditButton()]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Tests closing all the regular tabs with "Close All" button and then undoing
// the action.
- (void)testUndoCloseAllRegularTabs {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }

  // Create tabs.
  CreatePinnedTabs(2, self.testServer);
  CreateRegularTabs(2, self.testServer);

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Verify regular tabs are present.
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab0")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab1")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify last regular tab is selected.
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab1")]
      assertWithMatcher:grey_selected()];

  // Tap on "Edit" button.
  [[EarlGrey selectElementWithMatcher:GetMatcherForEditButton()]
      performAction:grey_tap()];

  // Tap on "Close All Tabs" menu action.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridEditMenuCloseAllButton()]
      performAction:grey_tap()];

  // Verify regular tabs are not present.
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab0")]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab1")]
      assertWithMatcher:grey_nil()];

  // Verify last pinned tab is selected.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab1")]
      assertWithMatcher:grey_selected()];

  // Verify "Edit" button becomes an "Undo" button.
  [[EarlGrey selectElementWithMatcher:GetMatcherForEditButton()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:GetMatcherForUndoButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on "Undo" button.
  [[EarlGrey selectElementWithMatcher:GetMatcherForUndoButton()]
      performAction:grey_tap()];

  // Verify regular tabs are present.
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab0")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab1")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify last regular tab is selected.
  [[EarlGrey selectElementWithMatcher:GetMatcherForRegularCellWithTitle(
                                          @"RegularTab1")]
      assertWithMatcher:grey_selected()];

  [self waitForAnimationCompletionWithMacther:GetMatcherForEditButton()];

  // Verify "Undo" button becomes an "Edit" button.
  [[EarlGrey selectElementWithMatcher:GetMatcherForUndoButton()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:GetMatcherForEditButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests scrolling of the pinned tabs collection.
- (void)testPinnedTabsScrolling {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }

  // Create tabs.
  CreatePinnedTabs(6, self.testServer);
  CreateRegularTabs(1, self.testServer);

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Verify that the first pinned tab is visible.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab5")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the last pinned tab is not visible.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab0")]
      assertWithMatcher:grey_notVisible()];

  // Scroll the pinned tabs.
  [[EarlGrey selectElementWithMatcher:GetMatcherForPinnedView()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeLeft)];

  // Verify that the first pinned tab is not visible.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab5")]
      assertWithMatcher:grey_notVisible()];

  // Verify that the last pinned tab is visible.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab0")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
