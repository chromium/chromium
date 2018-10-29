// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_egtest_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::TabGridDoneButton;
using chrome_test_util::TabGridIncognitoTabsPanelButton;
using chrome_test_util::TabGridNewIncognitoTabButton;
using chrome_test_util::TabGridNewTabButton;
using chrome_test_util::TabGridOpenButton;
using chrome_test_util::TabGridOpenTabsPanelButton;
// using chrome_test_util::TabletTabSwitcherCloseButton;
// using chrome_test_util::TabletTabSwitcherIncognitoTabsPanelButton;
// using chrome_test_util::TabletTabSwitcherNewIncognitoTabButton;
// using chrome_test_util::TabletTabSwitcherNewTabButton;
// using chrome_test_util::TabletTabSwitcherOpenButton;
// using chrome_test_util::TabletTabSwitcherOpenTabsPanelButton;

namespace {

// Returns the tab model for non-incognito tabs.
TabModel* GetNormalTabModel() {
  return [[chrome_test_util::GetMainController() browserViewInformation]
      mainTabModel];
}

// Shows the tab switcher by tapping the switcher button.  Works on both phone
// and tablet.
void ShowTabSwitcher() {
  id<GREYMatcher> matcher = TabGridOpenButton();
  DCHECK(matcher);

  // Perform a tap with a timeout. Occasionally EG doesn't sync up properly to
  // the animations of tab switcher, so it is necessary to poll here.
  GREYCondition* tapTabSwitcher =
      [GREYCondition conditionWithName:@"Tap tab switcher button"
                                 block:^BOOL {
                                   NSError* error;
                                   [[EarlGrey selectElementWithMatcher:matcher]
                                       performAction:grey_tap()
                                               error:&error];
                                   return error == nil;
                                 }];

  // Wait until 2 seconds for the tap.
  BOOL hasClicked = [tapTabSwitcher waitWithTimeout:2];
  GREYAssertTrue(hasClicked, @"Tab switcher could not be clicked.");
}

// Hides the tab switcher by tapping the switcher button.  Works on both phone
// and tablet.
void ShowTabViewController() {
  id<GREYMatcher> matcher = TabGridDoneButton();
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
}

// Selects and focuses the tab with the given |title|.
void SelectTab(NSString* title) {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(title),
                                          grey_accessibilityTrait(
                                              UIAccessibilityTraitStaticText),
                                          nil)] performAction:grey_tap()];
}

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

}  // namespace

@interface TabSwitcherTransitionTestCase : ChromeTestCase
@end

// NOTE: The test cases before are not totally independent.  For example, the
// setup steps for testEnterTabSwitcherWithOneIncognitoTab first close the last
// normal tab and then open a new incognito tab, which are both scenarios
// covered by other tests.  A single programming error may cause multiple tests
// to fail.
@implementation TabSwitcherTransitionTestCase

// Rotate the device back to portrait if needed, since some tests attempt to run
// in landscape.
- (void)tearDown {
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                           errorOrNil:nil];
  [super tearDown];
}

// Sets up the EmbeddedTestServer as needed for tests.
- (void)setUpTestServer {
  self.testServer->RegisterDefaultHandler(
      base::Bind(net::test_server::HandlePrefixedRequest, "/querytitle",
                 base::Bind(&HandleQueryTitle)));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");
}

// Returns the URL for a test page with the given |title|.
- (GURL)makeURLForTitle:(NSString*)title {
  return self.testServer->GetURL("/querytitle?" +
                                 base::SysNSStringToUTF8(title));
}

// Tests entering the tab switcher when one normal tab is open.
- (void)testEnterSwitcherWithOneNormalTab {
  ShowTabSwitcher();
}

// Tests entering the tab switcher when more than one normal tab is open.
- (void)testEnterSwitcherWithMultipleNormalTabs {
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGreyUI openNewTab];

  ShowTabSwitcher();
}

// Tests entering the tab switcher when one incognito tab is open.
- (void)testEnterSwitcherWithOneIncognitoTab {
  [ChromeEarlGreyUI openNewIncognitoTab];
  [GetNormalTabModel() closeAllTabs];

  ShowTabSwitcher();
}

// Tests entering the tab switcher when more than one incognito tab is open.
- (void)testEnterSwitcherWithMultipleIncognitoTabs {
  [ChromeEarlGreyUI openNewIncognitoTab];
  [GetNormalTabModel() closeAllTabs];
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGreyUI openNewIncognitoTab];

  ShowTabSwitcher();
}

// Tests entering the switcher when multiple tabs of both types are open.
- (void)testEnterSwitcherWithNormalAndIncognitoTabs {
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGreyUI openNewIncognitoTab];

  ShowTabSwitcher();
}

// Tests entering the tab switcher by closing the last normal tab.
- (void)testEnterSwitcherByClosingLastNormalTab {
  chrome_test_util::CloseAllTabsInCurrentMode();
}

// Tests entering the tab switcher by closing the last incognito tab.
- (void)testEnterSwitcherByClosingLastIncognitoTab {
  [ChromeEarlGreyUI openNewIncognitoTab];
  [GetNormalTabModel() closeAllTabs];

  chrome_test_util::CloseAllTabsInCurrentMode();
}

// Tests exiting the switcher by tapping the switcher button.
- (void)testLeaveSwitcherWithSwitcherButton {
  NSString* tab1_title = @"NormalTab1";
  [self setUpTestServer];

  // Load a test URL in the current tab.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab1_title]];

  // Enter and leave the switcher.
  ShowTabSwitcher();
  ShowTabViewController();

  // Verify that the original tab is visible again.
  [ChromeEarlGrey
      waitForWebViewContainingText:base::SysNSStringToUTF8(tab1_title)];
}

// Tests exiting the switcher by tapping the new tab button or selecting new tab
// from the menu (on phone only).
- (void)testLeaveSwitcherByOpeningNewNormalTab {
  NSString* tab1_title = @"NormalTab1";
  [self setUpTestServer];

  // Enter the switcher and open a new tab using the new tab button.
  ShowTabSwitcher();
  id<GREYMatcher> matcher = TabGridNewTabButton();
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];

  // Load a URL in this newly-created tab and verify that the tab is visible.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab1_title]];
  [ChromeEarlGrey
      waitForWebViewContainingText:base::SysNSStringToUTF8(tab1_title)];
}

// Tests exiting the switcher by tapping the new incognito tab button or
// selecting new incognito tab from the menu (on phone only).
- (void)testLeaveSwitcherByOpeningNewIncognitoTab {
  NSString* tab1_title = @"IncognitoTab1";
  [self setUpTestServer];

  // Set up by creating a new incognito tab and closing all normal tabs.
  [ChromeEarlGreyUI openNewIncognitoTab];
  [GetNormalTabModel() closeAllTabs];

  // Enter the switcher and open a new incognito tab using the new tab button.
  ShowTabSwitcher();
  id<GREYMatcher> matcher = TabGridNewIncognitoTabButton();
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];

  // Load a URL in this newly-created tab and verify that the tab is visible.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab1_title]];
  [ChromeEarlGrey
      waitForWebViewContainingText:base::SysNSStringToUTF8(tab1_title)];
}

// Tests exiting the switcher by opening a new tab in the other tab model.
- (void)testLeaveSwitcherByOpeningTabInOtherMode {
  NSString* normal_title = @"NormalTab";
  NSString* incognito_title = @"IncognitoTab";
  [self setUpTestServer];

  // Go from normal mode to incognito mode.
  ShowTabSwitcher();
  [[EarlGrey selectElementWithMatcher:TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridNewIncognitoTabButton()]
      performAction:grey_tap()];

  // Load a URL in this newly-created tab and verify that the tab is visible.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:incognito_title]];
  [ChromeEarlGrey
      waitForWebViewContainingText:base::SysNSStringToUTF8(incognito_title)];

  // Go from incognito mode to normal mode.
  ShowTabSwitcher();
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridNewTabButton()]
      performAction:grey_tap()];

  // Load a URL in this newly-created tab and verify that the tab is visible.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:normal_title]];
  [ChromeEarlGrey
      waitForWebViewContainingText:base::SysNSStringToUTF8(normal_title)];
}

// Tests exiting the tab switcher by selecting a normal tab.
- (void)testLeaveSwitcherBySelectingNormalTab {
  NSString* tab1_title = @"NormalTabLongerStringForTest1";
  NSString* tab2_title = @"NormalTabLongerStringForTest2";
  NSString* tab3_title = @"NormalTabLongerStringForTest3";
  [self setUpTestServer];

  // Create a few tabs and give them all unique titles.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab1_title]];
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab2_title]];
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab3_title]];

  ShowTabSwitcher();
  SelectTab(tab1_title);
  [ChromeEarlGrey
      waitForWebViewContainingText:base::SysNSStringToUTF8(tab1_title)];

  ShowTabSwitcher();
  SelectTab(tab3_title);
  [ChromeEarlGrey
      waitForWebViewContainingText:base::SysNSStringToUTF8(tab3_title)];
}

// Tests exiting the tab switcher by selecting an incognito tab.
- (void)testLeaveSwitcherBySelectingIncognitoTab {
  NSString* tab1_title = @"IncognitoTab1";
  NSString* tab2_title = @"IncognitoTab2";
  NSString* tab3_title = @"IncognitoTab3";
  [self setUpTestServer];

  // Create a few tabs and give them all unique titles.
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab1_title]];
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab2_title]];
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab3_title]];
  [GetNormalTabModel() closeAllTabs];

  ShowTabSwitcher();
  SelectTab(tab1_title);
  [ChromeEarlGrey
      waitForWebViewContainingText:base::SysNSStringToUTF8(tab1_title)];

  ShowTabSwitcher();
  SelectTab(tab3_title);
  [ChromeEarlGrey
      waitForWebViewContainingText:base::SysNSStringToUTF8(tab3_title)];
}

// Tests exiting the tab switcher by selecting a tab in the other tab model.
- (void)testLeaveSwitcherBySelectingTabInOtherMode {
  NSString* normal_title = @"NormalTabLongerStringForTest";
  NSString* incognito_title = @"IncognitoTab";
  [self setUpTestServer];

  // Create a few tabs and give them all unique titles.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:normal_title]];
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:incognito_title]];

  ShowTabSwitcher();
  // Switch to the normal panel and select the one tab that is there.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  SelectTab(normal_title);
  [ChromeEarlGrey
      waitForWebViewContainingText:base::SysNSStringToUTF8(normal_title)];

  ShowTabSwitcher();
  // Switch to the incognito panel and select the one tab that is there.
  [[EarlGrey selectElementWithMatcher:TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];

  SelectTab(incognito_title);
  [ChromeEarlGrey
      waitForWebViewContainingText:base::SysNSStringToUTF8(incognito_title)];
}

// Tests switching back and forth between the normal and incognito BVCs.
- (void)testSwappingBVCModesWithoutEnteringSwitcher {
  // Opening a new tab from the menu will force a change in BVC.
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGreyUI openNewTab];
}

// Tests switching back and forth between the normal and incognito BVCs many
// times.  This is a regression test for https://crbug.com/851954.
- (void)testSwappingBVCModesManyTimesWithoutEnteringSwitcher {
  for (int ii = 0; ii < 10; ++ii) {
    // Opening a new tab from the menu will force a change in BVC.
    [ChromeEarlGreyUI openNewIncognitoTab];
    [ChromeEarlGreyUI openNewTab];
  }
}

// Tests rotating the device while the switcher is not active.  This is a
// regression test case for https://crbug.com/789975.
- (void)testRotationsWhileSwitcherIsNotActive {
  NSString* tab_title = @"NormalTabLongerStringForRotationTest";
  [self setUpTestServer];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab_title]];

  // Show the tab switcher and return to the BVC, in portrait.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                           errorOrNil:nil];
  ShowTabSwitcher();
  SelectTab(tab_title);
  [ChromeEarlGrey
      waitForWebViewContainingText:base::SysNSStringToUTF8(tab_title)];

  // Show the tab switcher and return to the BVC, in landscape.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                           errorOrNil:nil];
  ShowTabSwitcher();
  SelectTab(tab_title);
  [ChromeEarlGrey
      waitForWebViewContainingText:base::SysNSStringToUTF8(tab_title)];

  // Show the tab switcher and return to the BVC, in portrait.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                           errorOrNil:nil];
  ShowTabSwitcher();
  SelectTab(tab_title);
  [ChromeEarlGrey
      waitForWebViewContainingText:base::SysNSStringToUTF8(tab_title)];
}

@end
