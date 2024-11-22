// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/feature_list.h"
#import "components/data_sharing/public/features.h"
#import "ios/chrome/browser/share_kit/model/test_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_eg_utils.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using chrome_test_util::CreateTabGroupAtIndex;
using chrome_test_util::ManageGroupButton;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::NavigationBarSaveButton;
using chrome_test_util::OpenTabGroupAtIndex;
using chrome_test_util::ShareGroupButton;
using chrome_test_util::TabGridGroupCellAtIndex;

namespace {

// Put the number at the beginning to avoid issues with sentence case, as the
// keyboard default can differ iPhone vs iPad, simulator vs device.
NSString* const kGroup1Name = @"1group";

// Matcher for the Share flow view.
id<GREYMatcher> FakeShareFlowView() {
  return grey_accessibilityID(kFakeShareFlowIdentifier);
}

// Matcher for the Manage flow view.
id<GREYMatcher> FakeManageFlowView() {
  return grey_accessibilityID(kFakeManageFlowIdentifier);
}

// Matcher for the Join flow view.
id<GREYMatcher> FakeJoinFlowView() {
  return grey_accessibilityID(kFakeJoinFlowIdentifier);
}

// Returns the completely configured AppLaunchConfiguration (i.e. setting all
// the underlying feature dependencies), with the Shared Tab Groups flavor as a
// parameter.
AppLaunchConfiguration SharedTabGroupAppLaunchConfiguration(
    const base::Feature& shared_tab_group_flavor) {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabGroupsIPad);
  config.features_enabled.push_back(kModernTabStrip);
  config.features_enabled.push_back(kTabGroupSync);
  config.features_enabled.push_back(shared_tab_group_flavor);
  // Add the flag to use FakeTabGroupSyncService.
  config.additional_args.push_back(
      "--" + std::string(test_switches::kEnableFakeTabGroupSyncService));
  return config;
}

// Shares the group at `index`.
void ShareGroupAtIndex(int index) {
  // Share the first group.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(index)]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      performAction:grey_tap()];

  // Verify that this opened the fake Share flow.
  [[EarlGrey selectElementWithMatcher:FakeShareFlowView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Actually share the group.
  [[EarlGrey selectElementWithMatcher:NavigationBarSaveButton()]
      performAction:grey_tap()];
}

}  // namespace

// Test Shared Tab Groups feature (with group creation access).
@interface SharedTabGroupsTestCase : ChromeTestCase
@end

@implementation SharedTabGroupsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  return SharedTabGroupAppLaunchConfiguration(
      data_sharing::features::kDataSharingFeature);
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey
      setUserDefaultsObject:@YES
                     forKey:kSharedTabGroupUserEducationShownOnceKey];
}

// Tests that the user education is shown in the grid only once.
- (void)testUserEducationInGrid {
  [ChromeEarlGrey
      removeUserDefaultsObjectForKey:kSharedTabGroupUserEducationShownOnceKey];

  [ChromeEarlGreyUI openTabGrid];
  CreateTabGroupAtIndex(0, kGroup1Name);

  ShareGroupAtIndex(0);

  OpenTabGroupAtIndex(0);

  id<GREYMatcher> educationScreen =
      grey_accessibilityID(kSharedTabGroupUserEducationAccessibilityIdentifier);

  // The user education screen is shown.
  [[EarlGrey selectElementWithMatcher:educationScreen]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Dismiss it, go back and re-enter.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConfirmationAlertPrimaryActionAccessibilityIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:educationScreen];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGroupBackButton()]
      performAction:grey_tap()];
  OpenTabGroupAtIndex(0);

  // The user education screen is not shown.
  [[EarlGrey selectElementWithMatcher:educationScreen]
      assertWithMatcher:grey_nil()];

  GREYAssert(
      [ChromeEarlGrey
          userDefaultsObjectForKey:kSharedTabGroupUserEducationShownOnceKey],
      @"The user default hasn't been updated");
}

// Checks opening the Share flow from the Tab Grid and cancelling.
- (void)testShareGroupButCancel {
  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Creates a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Share the first group.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      performAction:grey_tap()];

  // Verify that this opened the fake Share flow.
  [[EarlGrey selectElementWithMatcher:FakeShareFlowView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Cancel the Share flow.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Verify that it closed the Share flow.
  [[EarlGrey selectElementWithMatcher:FakeShareFlowView()]
      assertWithMatcher:grey_notVisible()];

  // Verify that the group is not shared by checking that the context menu
  // offers to Share rather than Manage the group.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ManageGroupButton()]
      assertWithMatcher:grey_notVisible()];
}

// Checks opening the Share flow from the Tab Grid and actually sharing. Then
// checks opening the Manage flow.
- (void)testShareGroupAndManageGroup {
  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Creates a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Share the first group.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      performAction:grey_tap()];

  // Verify that this opened the fake Share flow.
  [[EarlGrey selectElementWithMatcher:FakeShareFlowView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Actually share the group.
  [[EarlGrey selectElementWithMatcher:NavigationBarSaveButton()]
      performAction:grey_tap()];

  // Verify that it closed the Share flow.
  [[EarlGrey selectElementWithMatcher:FakeShareFlowView()]
      assertWithMatcher:grey_notVisible()];

  // Verify that the group is shared by checking that the context menu offers to
  // Manage rather than Share the group.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:ManageGroupButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      assertWithMatcher:grey_notVisible()];

  // Manage the group.
  [[EarlGrey selectElementWithMatcher:ManageGroupButton()]
      performAction:grey_tap()];

  // Verify that it opened the Manage flow.
  [[EarlGrey selectElementWithMatcher:FakeManageFlowView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close the Manage flow.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Verify that it closed the Manage flow.
  [[EarlGrey selectElementWithMatcher:FakeManageFlowView()]
      assertWithMatcher:grey_notVisible()];
}

// Checks that the user with JoinOnly can trigger the Join flow.
- (void)testJoinGroup {
  GURL joinGroupURL =
      GURL("https://www.chromium.org/data_sharing/"
           "?group_id=resources%2F3bebf45000000000%2Fe%2F50cc3ac28e000000&"
           "token_blob=CggHBicxA_slvxIWR2RvcXIzclJGR1E5eXQ0RUdpN2M3Zw");
  [ChromeEarlGrey loadURL:joinGroupURL waitForCompletion:NO];

  // Verify that it opened the Join flow.
  [[EarlGrey selectElementWithMatcher:FakeJoinFlowView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close the Join flow.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Verify that it closed the Join flow.
  [[EarlGrey selectElementWithMatcher:FakeJoinFlowView()]
      assertWithMatcher:grey_notVisible()];
}

@end

// Test Shared Tab Groups feature (with group joining access only.).
@interface SharedTabGroupsJoinOnlyTestCase : ChromeTestCase
@end

@implementation SharedTabGroupsJoinOnlyTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  return SharedTabGroupAppLaunchConfiguration(
      data_sharing::features::kDataSharingJoinOnly);
}

// Checks that the user with JoinOnly rights can't start the Share flow from
// Tab Grid.
- (void)testCantShareGroup {
  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Creates a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Try to share the first group.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_longPress()];

  // Verify that there is no Share or Manage button.
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:ManageGroupButton()]
      assertWithMatcher:grey_nil()];
}

// Checks that the user with JoinOnly can trigger the Join flow.
- (void)testJoinGroup {
  GURL joinGroupURL =
      GURL("https://www.chromium.org/data_sharing/"
           "?group_id=resources%2F3bebf45000000000%2Fe%2F50cc3ac28e000000&"
           "token_blob=CggHBicxA_slvxIWR2RvcXIzclJGR1E5eXQ0RUdpN2M3Zw");
  [ChromeEarlGrey loadURL:joinGroupURL waitForCompletion:NO];

  // Verify that it opened the Join flow.
  [[EarlGrey selectElementWithMatcher:FakeJoinFlowView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close the Join flow.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Verify that it closed the Join flow.
  [[EarlGrey selectElementWithMatcher:FakeJoinFlowView()]
      assertWithMatcher:grey_notVisible()];
}

@end
