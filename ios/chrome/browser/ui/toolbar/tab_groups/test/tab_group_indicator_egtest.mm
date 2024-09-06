// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/test/query_title_server_util.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ContextMenuItemWithAccessibilityLabel;
using chrome_test_util::CreateTabGroupCreateButton;
using chrome_test_util::TabGridCellAtIndex;
using chrome_test_util::TabGroupCreationView;

namespace {

NSString* const kTab1Title = @"Tab1";
NSString* const kTab2Title = @"Tab2";

// Matcher for the tab group indicator view.
id<GREYMatcher> TabGroupIndicatorViewMatcher() {
  return grey_accessibilityID(kTabGroupIndicatorViewIdentifier);
}

// Returns a matcher for the tab group indicator view with `title` as title.
id<GREYMatcher> TabGroupIndicatorViewMatcherWithGroupTitle(NSString* title) {
  return grey_allOf(
      grey_ancestor(grey_accessibilityID(kTabGroupIndicatorViewIdentifier)),
      grey_accessibilityLabel(title), grey_sufficientlyVisible(), nil);
}

// Returns a matcher for a menu button with `accessibility_label` as
// accessibility label.
id<GREYMatcher> MenuButtonMatcher(int accessibility_label_id) {
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabelId(accessibility_label_id),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

// Displays the tab cell context menu by long pressing at the tab cell at
// `tab_cell_index`.
void DisplayContextMenuForTabCellAtIndex(int tab_cell_index) {
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(tab_cell_index)]
      performAction:grey_longPress()];
}

// Creates a group with default title from a tab cell at index `tab_cell_index`
// when no group is in the grid.
void CreateDefaultFirstGroupFromTabCellAtIndex(int tab_cell_index) {
  DisplayContextMenuForTabCellAtIndex(tab_cell_index);
  [[EarlGrey
      selectElementWithMatcher:
          ContextMenuItemWithAccessibilityLabel(l10n_util::GetPluralNSStringF(
              IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP, 1))]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGroupCreationView()];
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButton()]
      performAction:grey_tap()];
}

}  // namespace

// Tests for the tab group indicator.
@interface TabGroupIndicatorTestCase : ChromeTestCase
@end

@implementation TabGroupIndicatorTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabGroupsInGrid);
  config.features_enabled.push_back(kTabGroupsIPad);
  config.features_enabled.push_back(kModernTabStrip);
  config.features_enabled.push_back(kTabGroupSync);
  config.features_enabled.push_back(kTabGroupIndicator);
  return config;
}

- (void)setUp {
  [super setUp];
  RegisterQueryTitleHandler(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");
}

// Tests that the tab group indicator view is visible when the active tab is
// grouped.
- (void)testTabGroupIndicatorVisibility {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"On iPad, the tab indicator is not displauyed if "
                           @"the tab strip is visible.");
  }

  // Check that the indicator is not visible.
  [[EarlGrey selectElementWithMatcher:TabGroupIndicatorViewMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Create a tab cell.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  // Create a group.
  CreateDefaultFirstGroupFromTabCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Check that the indicator is visible.
  [[EarlGrey
      selectElementWithMatcher:TabGroupIndicatorViewMatcherWithGroupTitle(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the indicator is not visible in landscape.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];
  [[EarlGrey selectElementWithMatcher:TabGroupIndicatorViewMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Check that the indicator is visible when switching back to portrait.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  [[EarlGrey
      selectElementWithMatcher:TabGroupIndicatorViewMatcherWithGroupTitle(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the tab group indicator view is visible when the active tab is
// grouped.
- (void)testTabGroupIndicatorNotVisibleOnIpad {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped on iPhone.");
  }

  // Check that the indicator is not visible.
  [[EarlGrey selectElementWithMatcher:TabGroupIndicatorViewMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Create a tab cell.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  // Create a group.
  CreateDefaultFirstGroupFromTabCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Check that the indicator is still not visible.
  [[EarlGrey selectElementWithMatcher:TabGroupIndicatorViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that opening a new tab in the group from the tab group indicator menu
// works.
- (void)testTabGroupIndicatorMenuOpenNewTab {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"On iPad, the tab indicator is not displauyed if "
                           @"the tab strip is visible.");
  }

  // Create a tab cell.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  // Create a group.
  CreateDefaultFirstGroupFromTabCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Open the tab group indicator menu .
  [[EarlGrey
      selectElementWithMatcher:TabGroupIndicatorViewMatcherWithGroupTitle(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      performAction:grey_tap()];

  // Tap on the "New tab in group" button.
  [[EarlGrey
      selectElementWithMatcher:MenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_NEWTABINGROUP)]
      performAction:grey_tap()];

  // Check that there are now two tabs and the current tab has changed.
  GREYAssertEqual(2, [ChromeEarlGrey mainTabCount],
                  @"Expected 2 tabs to be present.");
  NSString* newTabTitle = [ChromeEarlGrey currentTabTitle];
  GREYAssertNotEqual(kTab1Title, newTabTitle,
                     @"New current tab should have a different title");
}

@end
