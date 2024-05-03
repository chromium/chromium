// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

NSString* const kTab1Title = @"Tab1";
NSString* const kTab2Title = @"Tab2";

// Put the number at the beginning to avoid issues with sentence case.
NSString* const kGroupName = @"1group";

// Matcher for the create button in the tab group creation view.
id<GREYMatcher> CreateTabGroupCreateButtonMatcher() {
  return grey_allOf(grey_accessibilityID(kCreateTabGroupCreateButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for tab group strip cell for the given `group_name`.
id<GREYMatcher> TabGroupStripCellMatcher(NSString* group_name) {
  return grey_allOf(grey_accessibilityLabel(group_name),
                    grey_kindOfClassName(@"TabStripGroupCell"),
                    grey_sufficientlyVisible(), nil);
}

// Returns the matcher for the tab group creation view.
id<GREYMatcher> GroupCreationViewMatcher() {
  return grey_allOf(grey_accessibilityID(kCreateTabGroupViewIdentifier),
                    grey_minimumVisiblePercent(0.5), nil);
}

// Matcher for the text field in the tab group creation view.
id<GREYMatcher> CreateTabGroupTextFieldMatcher() {
  return grey_allOf(grey_accessibilityID(kCreateTabGroupTextFieldIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the cancel button in the tab group creation view.
id<GREYMatcher> CreateTabGroupCancelButtonMatcher() {
  return grey_allOf(grey_accessibilityID(kCreateTabGroupCancelButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Opens the tab group creation view using the long press context menu for the
// tab at `index`.
void OpenTabGroupCreationViewUsingLongPressForCellAtIndex(int index) {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabStripCellAtIndex(index)]
      performAction:grey_longPress()];

  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetPluralNSStringF(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP,
                                   1))] performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:GroupCreationViewMatcher()];
}

// Sets the tab group name in the tab group creation view.
void SetTabGroupCreationName(NSString* group_name) {
  [[EarlGrey selectElementWithMatcher:CreateTabGroupTextFieldMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:group_name flags:0];
}

}  // namespace

// Tests for the tab strip shown on iPad.
@interface TabStripGroupTestCase : ChromeTestCase
@end

@implementation TabStripGroupTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabGroupsInGrid);
  config.features_enabled.push_back(kTabGroupsIPad);
  config.features_enabled.push_back(kModernTabStrip);
  return config;
}

// Tests creating a tab group and opening the grouped tab.
- (void)testCompleteTabGroupCreation {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  // Open the creation view.
  OpenTabGroupCreationViewUsingLongPressForCellAtIndex(0);
  SetTabGroupCreationName(kGroupName);

  // Confirm the creation.
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButtonMatcher()]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:GroupCreationViewMatcher()];
  [[EarlGrey selectElementWithMatcher:TabGroupStripCellMatcher(kGroupName)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests canceling a tab group creation.
- (void)testCancelTabGroupCreation {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  // Open the creation view.
  OpenTabGroupCreationViewUsingLongPressForCellAtIndex(0);
  SetTabGroupCreationName(kGroupName);

  // Cancel the creation.
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCancelButtonMatcher()]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:GroupCreationViewMatcher()];
  [[EarlGrey selectElementWithMatcher:TabGroupStripCellMatcher(kGroupName)]
      assertWithMatcher:grey_nil()];
}

@end
