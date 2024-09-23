// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/test/query_title_server_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/device_form_factor.h"

using chrome_test_util::CloseTabMenuButton;
using chrome_test_util::TabGridDoneButton;
using chrome_test_util::TabGridIncognitoTabsPanelButton;
using chrome_test_util::TabGridNewIncognitoTabButton;
using chrome_test_util::TabGridNewTabButton;
using chrome_test_util::TabGridOpenTabsPanelButton;
using chrome_test_util::TabGridOtherDevicesPanelButton;
using chrome_test_util::TabGridSearchCancelButton;
using chrome_test_util::TabGridSearchModeToolbar;
using chrome_test_util::TabGridSearchTabsButton;
using chrome_test_util::TabGridThirdPanelButton;

namespace {

// Hides the tab switcher by tapping the switcher button.  Works on both phone
// and tablet.
void ShowTabViewController() {
  id<GREYMatcher> matcher = TabGridDoneButton();
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
}

// Selects and focuses the tab with the given `title`.
void SelectTab(NSString* title) {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(title),
                                          grey_ancestor(grey_kindOfClassName(
                                              @"GridCell")),
                                          grey_accessibilityTrait(
                                              UIAccessibilityTraitStaticText),
                                          grey_hidden(NO), nil)]
      performAction:grey_tap()];
}

// Expects that the total number of samples in histogram `histogram` grew by
// `expected_count` since the HistogramTester was created.
void ExpectIdleHistogramCount(const char* histogram, int expected_count) {
  NSError* error = [MetricsAppInterface expectTotalCount:expected_count
                                            forHistogram:@(histogram)];
  GREYAssertNil(error, error.description);
}

// Expects that the total number of samples in histogram `histogram` for bucket
// `bucket` grew by `expected_count` since the HistogramTester was created.
void ExpectIdleHistogramBucketCount(const char* histogram,
                                    int expected_count,
                                    BOOL bucket) {
  NSError* error = [MetricsAppInterface expectCount:expected_count
                                          forBucket:static_cast<int>(bucket)
                                       forHistogram:@(histogram)];
  GREYAssertNil(error, error.description);
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

- (void)setUp {
  [super setUp];

  // Observe histograms in tests.
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
}

// Rotate the device back to portrait if needed, since some tests attempt to run
// in landscape.
- (void)tearDown {
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];

  // Release the histogram tester.
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
  [super tearDown];
}

// Sets up the EmbeddedTestServer as needed for tests.
- (void)setUpTestServer {
  RegisterQueryTitleHandler(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");
}

// Returns the URL for a test page with the given `title`.
- (GURL)makeURLForTitle:(NSString*)title {
  return GetQueryTitleURL(self.testServer, title);
}

// Tests entering the tab switcher when one normal tab is open.
- (void)testEnterSwitcherWithOneNormalTab {
  [ChromeEarlGrey showTabSwitcher];
}

// Tests entering the tab switcher when more than one normal tab is open.
- (void)testEnterSwitcherWithMultipleNormalTabs {
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey showTabSwitcher];
}

// Tests entering the tab switcher when one incognito tab is open.
- (void)testEnterSwitcherWithOneIncognitoTab {
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGrey closeAllNormalTabs];
  [ChromeEarlGrey showTabSwitcher];
}

// Tests entering the tab switcher when more than one incognito tab is open.
- (void)testEnterSwitcherWithMultipleIncognitoTabs {
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGrey closeAllNormalTabs];
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGrey showTabSwitcher];
}

// Tests entering the switcher when multiple tabs of both types are open.
- (void)testEnterSwitcherWithNormalAndIncognitoTabs {
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGrey showTabSwitcher];
}

// Tests entering the tab switcher by closing the last normal tab.
- (void)testEnterSwitcherByClosingLastNormalTab {
  [ChromeEarlGrey closeAllTabsInCurrentMode];
}

// Tests entering the tab switcher by closing the last incognito tab.
- (void)testEnterSwitcherByClosingLastIncognitoTab {
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGrey closeAllNormalTabs];
  [ChromeEarlGrey closeAllTabsInCurrentMode];
}

// Tests exiting the switcher by tapping the switcher button.
- (void)testLeaveSwitcherWithSwitcherButton {
  NSString* tab1_title = @"NormalTab1";
  [self setUpTestServer];

  // Load a test URL in the current tab.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab1_title]];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);

  // Enter and leave the switcher.
  [ChromeEarlGrey showTabSwitcher];
  ShowTabViewController();

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 1);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 1, YES);

  // Verify that the original tab is visible again.
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(tab1_title)];
}

// Tests exiting the switcher by tapping the new tab button or selecting new tab
// from the menu (on phone only).
- (void)testLeaveSwitcherByOpeningNewNormalTab {
  NSString* tab1_title = @"NormalTab1";
  [self setUpTestServer];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);

  // Enter the switcher and open a new tab using the new tab button.
  [ChromeEarlGrey showTabSwitcher];
  id<GREYMatcher> matcher = TabGridNewTabButton();
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 1);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 1, NO);

  // Load a URL in this newly-created tab and verify that the tab is visible.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab1_title]];
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(tab1_title)];
}

// Tests exiting the switcher by tapping the new incognito tab button or
// selecting new incognito tab from the menu (on phone only).
- (void)testLeaveSwitcherByOpeningNewIncognitoTab {
  NSString* tab1_title = @"IncognitoTab1";
  [self setUpTestServer];

  // Set up by creating a new incognito tab and closing all normal tabs.
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGrey closeAllNormalTabs];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);

  // Enter the switcher and open a new incognito tab using the new tab button.
  [ChromeEarlGrey showTabSwitcher];
  id<GREYMatcher> matcher = TabGridNewIncognitoTabButton();
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 1);
  ExpectIdleHistogramBucketCount(
      kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 1, NO);

  // Load a URL in this newly-created tab and verify that the tab is visible.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab1_title]];
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(tab1_title)];
}

// Tests exiting the switcher by opening a new tab in the other tab model.
- (void)testLeaveSwitcherByOpeningTabInOtherMode {
  NSString* normal_title = @"NormalTab";
  NSString* incognito_title = @"IncognitoTab";
  [self setUpTestServer];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);

  // Go from normal mode to incognito mode.
  [ChromeEarlGrey showTabSwitcher];
  [[EarlGrey selectElementWithMatcher:TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridNewIncognitoTabButton()]
      performAction:grey_tap()];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 1);
  ExpectIdleHistogramBucketCount(
      kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 1, NO);

  // Load a URL in this newly-created tab and verify that the tab is visible.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:incognito_title]];
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(incognito_title)];

  // Go from incognito mode to normal mode.
  [ChromeEarlGrey showTabSwitcher];
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridNewTabButton()]
      performAction:grey_tap()];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 1);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 1);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 1, NO);
  ExpectIdleHistogramBucketCount(
      kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 1, NO);

  // Load a URL in this newly-created tab and verify that the tab is visible.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:normal_title]];
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(normal_title)];
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

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);

  [ChromeEarlGrey showTabSwitcher];
  SelectTab(tab1_title);
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(tab1_title)];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 1);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 1, NO);

  [ChromeEarlGrey showTabSwitcher];
  SelectTab(tab3_title);
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(tab3_title)];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 2);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 2, NO);

  [ChromeEarlGrey showTabSwitcher];
  SelectTab(tab3_title);
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(tab3_title)];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 3);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 1, YES);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 2, NO);
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
  [ChromeEarlGrey closeAllNormalTabs];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);

  [ChromeEarlGrey showTabSwitcher];
  SelectTab(tab1_title);
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(tab1_title)];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 1);
  ExpectIdleHistogramBucketCount(
      kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 1, NO);

  [ChromeEarlGrey showTabSwitcher];
  SelectTab(tab3_title);
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(tab3_title)];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 2);
  ExpectIdleHistogramBucketCount(
      kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 2, NO);

  [ChromeEarlGrey showTabSwitcher];
  SelectTab(tab3_title);
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(tab3_title)];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);
  ExpectIdleHistogramBucketCount(
      kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 1, YES);
  ExpectIdleHistogramBucketCount(
      kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 2, NO);
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

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);

  [ChromeEarlGrey showTabSwitcher];
  // Switch to the normal panel and select the one tab that is there.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  SelectTab(normal_title);
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(normal_title)];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 1);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 1, NO);

  [ChromeEarlGrey showTabSwitcher];
  // Switch to the incognito panel and select the one tab that is there.
  [[EarlGrey selectElementWithMatcher:TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];

  SelectTab(incognito_title);
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(incognito_title)];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 1);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 1);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 1, NO);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 1, NO);
}

// Tests exiting the tab switcher after switch back and forth between the normal
// page and the third page.
- (void)testLeaveSwitcherAfterVisitingThirdPanel {
  [self setUpTestServer];

  NSString* tab1_title = @"NormalTab1";

  // Load a test URL in the current tab.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab1_title]];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);

  [ChromeEarlGrey showTabSwitcher];

  // Switch to the third panel.
  [[EarlGrey selectElementWithMatcher:TabGridThirdPanelButton()]
      performAction:grey_tap()];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);

  // Switch back to the regular tabs panel and open the selected tab.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 1);
    ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleTabGroupsHistogram, 1,
                                   YES);
    ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  } else {
    ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 1);
    ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRecentTabsHistogram, 1,
                                   YES);
    ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);
  }

  SelectTab(tab1_title);

  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 1);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 1, YES);
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 1);
    ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleTabGroupsHistogram, 1,
                                   YES);
    ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  } else {
    ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 1);
    ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRecentTabsHistogram, 1,
                                   YES);
    ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);
  }
}

// Tests deleting a tab and exiting the tab switcher after switch back and forth
// between the normal page and the incognito page.
- (void)testDeleteAndLeaveSwitcherAfterVisitingIncognitoPage {
  NSString* tab1_title = @"NormalTabLongerStringForTest1";
  NSString* tab2_title = @"NormalTabLongerStringForTest2";
  [self setUpTestServer];

  // Create a few tabs and give them all unique titles.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab1_title]];
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab2_title]];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);

  [ChromeEarlGrey showTabSwitcher];

  // Close a tab and switch to the incognito page.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);

  // Switch back to the regular page and open the selected tab.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  SelectTab(tab2_title);

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 1);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 1, NO);
}

// Tests exiting the tab switcher after closing a normal tab.
- (void)testLeaveSwitcherAfterClosingNormalTab {
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

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);

  [ChromeEarlGrey showTabSwitcher];

  // Close a tab an open a non-selected tab.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(1)]
      performAction:grey_tap()];
  SelectTab(tab1_title);
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(tab1_title)];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 1);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 1, NO);

  [ChromeEarlGrey showTabSwitcher];

  // Close tab #3 and open the current selected tab.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(1)]
      performAction:grey_tap()];
  SelectTab(tab1_title);
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(tab1_title)];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 2);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 2, NO);
}

// Tests exiting the tab switcher by opening an incognito tab after closing a
// normal tab.
- (void)testLeaveSwitcherFromIncognitoTabAfterClosingNormalTab {
  NSString* tab1_title = @"NormalTabLongerStringForTest1";
  NSString* tab2_title = @"NormalTabLongerStringForTest2";
  [self setUpTestServer];

  // Create a few tabs and give them all unique titles.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab1_title]];
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab2_title]];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);

  [ChromeEarlGrey showTabSwitcher];

  // Close a regular tab and open an incognito page.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(1)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridNewIncognitoTabButton()]
      performAction:grey_tap()];

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 1);
  ExpectIdleHistogramBucketCount(
      kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 1, NO);
}

// Tests leaving tab grid after entering and exit the tab grid search mode.
- (void)testLeaveSwitcherAfterEnteringAndExittingSearch {
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);

  [ChromeEarlGrey showTabSwitcher];
  // Enter search mode and verify active.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridSearchModeToolbar()]
      assertWithMatcher:grey_notNil()];
  // Exit search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchCancelButton()]
      performAction:grey_tap()];
  // Leave switcher by tap "Done" button.
  ShowTabViewController();

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 1);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 1, NO);
}

// Tests leaving tab grid after long press on a tab.
- (void)testLeaveSwitcherAfterLongPressOnTab {
  NSString* title = @"NormalTabLongerStringForTest1";
  [self setUpTestServer];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:title]];
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);

  [ChromeEarlGrey showTabSwitcher];
  // Long press on the tab and dismiss context menu.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(title),
                                          grey_sufficientlyVisible(), nil)]
      atIndex:0] performAction:grey_longPress()];
  // Tap somewhere else to dismiss and leave switcher by tap "Done" button.
  [[EarlGrey selectElementWithMatcher:grey_keyWindow()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:TabGridDoneButton()];
  ShowTabViewController();

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 1);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 1, NO);
}

// Tests leaving tab grid after entering and exit inactive tabs grid.
- (void)testLeaveSwitcherAfterEnteringAndExittingInactiveTabs {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }
  // Mark the User Education screen as already-seen by default.
  [ChromeEarlGrey setUserDefaultsObject:@YES
                                 forKey:kInactiveTabsUserEducationShownOnceKey];
  NSString* title = @"NormalTabLongerStringForTest1";
  [self setUpTestServer];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:title]];

  // Relaunch with inactive tabs enabled.
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      "--enable-features=" + std::string(kTabInactivityThreshold.name) + ":" +
      kTabInactivityThresholdParameterName + "/" +
      kTabInactivityThresholdImmediateDemoParam);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);

  [ChromeEarlGrey showTabSwitcher];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kInactiveTabsButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:testing::NavigationBarBackButton()]
      performAction:grey_tap()];
  ShowTabViewController();

  ExpectIdleHistogramCount(kUMATabSwitcherIdleRecentTabsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleTabGroupsHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleIncognitoTabGridPageHistogram, 0);
  ExpectIdleHistogramCount(kUMATabSwitcherIdleRegularTabGridPageHistogram, 1);
  ExpectIdleHistogramBucketCount(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                 1, NO);
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
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  [ChromeEarlGrey showTabSwitcher];
  SelectTab(tab_title);
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(tab_title)];

  // Show the tab switcher and return to the BVC, in landscape.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];
  [ChromeEarlGrey showTabSwitcher];
  SelectTab(tab_title);
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(tab_title)];

  // Show the tab switcher and return to the BVC, in portrait.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  [ChromeEarlGrey showTabSwitcher];
  SelectTab(tab_title);
  [ChromeEarlGrey
      waitForWebStateContainingText:base::SysNSStringToUTF8(tab_title)];
}

@end
