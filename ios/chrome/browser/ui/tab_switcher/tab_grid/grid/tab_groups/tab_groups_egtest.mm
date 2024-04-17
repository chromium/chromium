// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Put the number at the beginning to avoid issues with sentence case.
NSString* const kGroupName = @"1group";

// Matcher for the tab group creation view.
id<GREYMatcher> CreateTabGroupViewMatcher() {
  return grey_allOf(grey_accessibilityID(kCreateTabGroupViewIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the text field in the tab group creation view.
id<GREYMatcher> CreateTabGroupTextFieldMatcher() {
  return grey_allOf(grey_accessibilityID(kCreateTabGroupTextFieldIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the create button in the tab group creation view.
id<GREYMatcher> CreateTabGroupCreateButtonMatcher() {
  return grey_allOf(grey_accessibilityID(kCreateTabGroupCreateButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the cancel button in the tab group creation view.
id<GREYMatcher> CreateTabGroupCancelButtonMatcher() {
  return grey_allOf(grey_accessibilityID(kCreateTabGroupCancelButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for tab group grid cell for the given `group_name`.
id<GREYMatcher> TabGroupGridCellMatcherMatcher(NSString* group_name) {
  return grey_allOf(grey_accessibilityLabel(group_name),
                    grey_kindOfClassName(@"GroupGridCell"),
                    grey_sufficientlyVisible(), nil);
}

// Checks that the tab group creation view is `visible`.
void WaitForVisibleTabGroupCreationView(bool visible) {
  GREYCondition* tabGroupCreationViewVisible = [GREYCondition
      conditionWithName:@"Wait for creation view update"
                  block:^BOOL {
                    NSError* error;
                    [[EarlGrey
                        selectElementWithMatcher:CreateTabGroupViewMatcher()]
                        assertWithMatcher:visible ? grey_notNil() : grey_nil()
                                    error:&error];
                    return error == nil;
                  }];

  GREYAssertTrue([tabGroupCreationViewVisible
                     waitWithTimeout:base::test::ios::kWaitForUIElementTimeout
                                         .InSecondsF()],
                 visible ? @"Tab Group creation view is not visible"
                         : @"Tab Group creation view is still visible");
}

// Opens the tab group creation view using the long press context menu for the
// tab at `index`.
void OpenTabGroupCreationViewUsingLongPressForCellAtIndex(int index) {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(index)]
      performAction:grey_longPress()];

  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetPluralNSStringF(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP,
                                   1))] performAction:grey_tap()];

  WaitForVisibleTabGroupCreationView(true);
}

// Sets the tab group name in the tab group creation view.
void SetTabGroupCreationName(NSString* group_name) {
  [[EarlGrey selectElementWithMatcher:CreateTabGroupTextFieldMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:group_name flags:0];
}

}  // namespace

// Test Tab Groups feature.
@interface TabGroupsTestCase : ChromeTestCase
@end

@implementation TabGroupsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabGroupsInGrid);
  config.features_enabled.push_back(kTabGroupsIPad);
  config.features_enabled.push_back(kModernTabStrip);
  return config;
}

// Tests that creates a tab group and opens the grouped tab.
- (void)testCompleteTabGroupCreation {
  [ChromeEarlGreyUI openTabGrid];

  // Open the creation view.
  OpenTabGroupCreationViewUsingLongPressForCellAtIndex(0);
  SetTabGroupCreationName(kGroupName);

  // Valid the creation.
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButtonMatcher()]
      performAction:grey_tap()];

  WaitForVisibleTabGroupCreationView(false);

  // Open the group.
  [[EarlGrey
      selectElementWithMatcher:TabGroupGridCellMatcherMatcher(kGroupName)]
      performAction:grey_tap()];

  // Open the tab.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::RegularTabGrid()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that cancels a tab group creation.
- (void)testCancelTabGroupCreation {
  [ChromeEarlGreyUI openTabGrid];

  // Open the creation view.
  OpenTabGroupCreationViewUsingLongPressForCellAtIndex(0);
  SetTabGroupCreationName(kGroupName);

  // Cancel the creation.
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCancelButtonMatcher()]
      performAction:grey_tap()];

  WaitForVisibleTabGroupCreationView(false);
  [[EarlGrey
      selectElementWithMatcher:TabGroupGridCellMatcherMatcher(kGroupName)]
      assertWithMatcher:grey_nil()];
}

@end
