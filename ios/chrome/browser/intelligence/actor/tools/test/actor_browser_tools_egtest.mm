// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/test/actor_app_interface.h"
#import "ios/chrome/browser/intelligence/actor/tools/test/actor_tools_base_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "url/gurl.h"

@interface ActorBrowserToolsTestCase : ActorToolsBaseTestCase
@end

@implementation ActorBrowserToolsTestCase

#pragma mark - NavigateTool Tests

// Tests that the navigate tool successfully navigates the active tab to a new
// URL.
- (void)testNavigateTool_worksOnForegroundTab {
  const GURL destinationURL = [self URLForHTML:"Hello"];

  optimization_guide::proto::Action action;
  optimization_guide::proto::NavigateAction* navigateAction =
      action.mutable_navigate();
  navigateAction->set_url(destinationURL.spec());
  navigateAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  [ChromeEarlGrey waitForWebStateContainingText:"Hello"];
}

// Tests that the navigate tool successfully navigates a background tab to a new
// URL without changing the active tab.
- (void)testNavigateTool_worksOnBackgroundTab {
  const GURL destinationURL = [self URLForHTML:"Hello"];

  // Open a new tab and navigate to a test page to easily distinguish from the
  // initial tab.
  NSString* backgroundTabID = [ChromeEarlGrey currentTabID];
  [ChromeEarlGrey openNewTab];
  NSString* foregroundTabID = [ChromeEarlGrey currentTabID];
  [ChromeEarlGrey loadURL:[self URLForHTML:"Google"]];
  [ChromeEarlGrey waitForWebStateContainingText:"Google"];

  optimization_guide::proto::Action action;
  optimization_guide::proto::NavigateAction* navigateAction =
      action.mutable_navigate();
  navigateAction->set_url(destinationURL.spec());
  navigateAction->set_tab_id(backgroundTabID.intValue);

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  // Verify that the browser did not change the active tab.
  GREYAssertEqualObjects(
      [ChromeEarlGrey currentTabID], foregroundTabID,
      @"Navigating the background tab changed the active tab.");

  // Switch back to the background tab to verify the navigation.
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:"Hello"];
}

#pragma mark - HistoryTool Tests

// Tests that the history tool successfully navigates the user back when the tab
// is on the foreground.
- (void)testHistoryBackTool_worksOnForegroundTab {
  [ChromeEarlGrey loadURL:[self URLForHTML:"PageA"]];
  [ChromeEarlGrey waitForWebStateContainingText:"PageA"];

  [ChromeEarlGrey loadURL:[self URLForHTML:"PageB"]];
  [ChromeEarlGrey waitForWebStateContainingText:"PageB"];

  optimization_guide::proto::Action action;
  action.mutable_back()->set_tab_id([ChromeEarlGrey currentTabID].intValue);

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  [ChromeEarlGrey waitForWebStateContainingText:"PageA"];
}

// Tests that the history tool successfully navigates the user back when the tab
// is on the background.
- (void)testHistoryBackTool_worksOnBackgroundTab {
  [ChromeEarlGrey loadURL:[self URLForHTML:"PageA"]];
  [ChromeEarlGrey waitForWebStateContainingText:"PageA"];

  [ChromeEarlGrey loadURL:[self URLForHTML:"PageB"]];
  [ChromeEarlGrey waitForWebStateContainingText:"PageB"];

  // Put the current tab on background by opening a new tab.
  NSString* backgroundTabID = [ChromeEarlGrey currentTabID];
  [ChromeEarlGrey openNewTab];
  NSString* foregroundTabID = [ChromeEarlGrey currentTabID];

  optimization_guide::proto::Action action;
  action.mutable_back()->set_tab_id(backgroundTabID.intValue);

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  // Verify that the browser did not change the active tab.
  GREYAssertEqualObjects(
      [ChromeEarlGrey currentTabID], foregroundTabID,
      @"Navigating the background tab changed the active tab.");

  // Switch back to the background tab to verify the navigation.
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:"PageA"];
}

// Tests that the history tool successfully navigates the user forward.
- (void)testHistoryForwardTool {
  [ChromeEarlGrey loadURL:[self URLForHTML:"PageA"]];
  [ChromeEarlGrey waitForWebStateContainingText:"PageA"];

  [ChromeEarlGrey loadURL:[self URLForHTML:"PageB"]];
  [ChromeEarlGrey waitForWebStateContainingText:"PageB"];

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:"PageA"];

  optimization_guide::proto::Action action;
  action.mutable_forward()->set_tab_id([ChromeEarlGrey currentTabID].intValue);

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  [ChromeEarlGrey waitForWebStateContainingText:"PageB"];
}

#pragma mark - TabManagementTool Tests

// Tests that the create tab tool successfully creates a new tab in the
// foreground.
- (void)testCreateTabTool_foreground {
  int initialTabCount = [ChromeEarlGrey mainTabCount];
  NSString* initialTabID = [ChromeEarlGrey currentTabID];

  optimization_guide::proto::Action action;
  action.mutable_create_tab()->set_foreground(true);
  action.mutable_create_tab()->set_window_id(
      [ActorAppInterface currentWindowID]);

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  int finalTabCount = [ChromeEarlGrey mainTabCount];
  GREYAssertEqual(finalTabCount, initialTabCount + 1,
                  @"Expected tab count to increase by 1, was %d -> %d",
                  initialTabCount, finalTabCount);

  NSString* finalTabID = [ChromeEarlGrey currentTabID];
  GREYAssertNotEqualObjects(
      finalTabID, initialTabID,
      @"Expected active tab to change (foreground tab created).");
}

// Tests that the create tab tool successfully creates a new tab in the
// background.
- (void)testCreateTabTool_background {
  int initialTabCount = [ChromeEarlGrey mainTabCount];
  NSString* initialTabID = [ChromeEarlGrey currentTabID];

  optimization_guide::proto::Action action;
  action.mutable_create_tab()->set_foreground(false);
  action.mutable_create_tab()->set_window_id(
      [ActorAppInterface currentWindowID]);

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  int finalTabCount = [ChromeEarlGrey mainTabCount];
  GREYAssertEqual(finalTabCount, initialTabCount + 1,
                  @"Expected tab count to increase by 1, was %d -> %d",
                  initialTabCount, finalTabCount);

  NSString* finalTabID = [ChromeEarlGrey currentTabID];
  GREYAssertEqualObjects(
      finalTabID, initialTabID,
      @"Expected active tab to remain the same (background tab created).");
}

// Tests that the activate tab tool successfully switches the active tab.
- (void)testActivateTabTool {
  // Create a new tab so we have at least 2 tabs open.
  [ChromeEarlGrey openNewTab];
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2u, @"Expected 2 tabs");

  // Get the IDs of the current active tab (1) and the first tab (0).
  NSString* tabId1 = [ChromeEarlGrey currentTabID];
  [ChromeEarlGrey selectTabAtIndex:0];
  NSString* tabId0 = [ChromeEarlGrey currentTabID];
  GREYAssertNotEqualObjects(tabId0, tabId1, @"Tab IDs should be different");
  // Currently tab 0 is active.
  GREYAssertEqualObjects([ChromeEarlGrey currentTabID], tabId0,
                         @"Expected tab 0 to be active");

  // Run the ActivateTab tool targeting tab 1.
  optimization_guide::proto::Action action;
  int32_t targetTabIdVal = [tabId1 intValue];
  action.mutable_activate_tab()->set_tab_id(targetTabIdVal);

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  // Verify that tab 1 is now the active tab.
  NSString* activeTabID = [ChromeEarlGrey currentTabID];
  GREYAssertEqualObjects(
      activeTabID, tabId1,
      @"Expected tab 1 to be active after ActivateTab action, but was %@",
      activeTabID);
}

// Tests that the close tab tool successfully closes a tab.
- (void)testCloseTabTool_success {
  [ChromeEarlGrey openNewTab];
  int initialTabCount = [ChromeEarlGrey mainTabCount];
  int targetTabID = [ChromeEarlGrey currentTabID].intValue;

  optimization_guide::proto::Action action;
  action.mutable_close_tab()->set_tab_id(targetTabID);

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  int finalTabCount = [ChromeEarlGrey mainTabCount];
  GREYAssertEqual(finalTabCount, initialTabCount - 1,
                  @"Expected tab count to decrease by 1, was %d -> %d",
                  initialTabCount, finalTabCount);
}

@end
