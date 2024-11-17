// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "components/data_sharing/public/features.h"
#import "ios/chrome/browser/share_kit/model/test_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_eg_utils.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using chrome_test_util::CreateTabGroupAtIndex;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::ShareGroupButton;
using chrome_test_util::TabGridGroupCellAtIndex;

namespace {

// Put the number at the beginning to avoid issues with sentence case, as the
// keyboard default can differ iPhone vs iPad, simulator vs device.
NSString* const kGroup1Name = @"1group";

id<GREYMatcher> FakeShareFlowView() {
  return grey_accessibilityID(kFakeShareFlowViewControllerIdentifier);
}

}  // namespace

// Test Shared Tab Groups feature (with group creation access).
@interface SharedTabGroupsTestCase : ChromeTestCase
@end

@implementation SharedTabGroupsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabGroupsIPad);
  config.features_enabled.push_back(kModernTabStrip);
  config.features_enabled.push_back(kTabGroupSync);
  config.features_enabled.push_back(
      data_sharing::features::kDataSharingFeature);
  return config;
}

// Checks opening the Share flow from the Tab Grid.
- (void)testShareGroup {
  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Creates a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Share the first group.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      performAction:grey_tap()];

  // Check that this opened the fake Share flow.
  [[EarlGrey selectElementWithMatcher:FakeShareFlowView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Cancel the Share flow.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Verify that it closed the Share flow.
  [[EarlGrey selectElementWithMatcher:FakeShareFlowView()]
      assertWithMatcher:grey_notVisible()];
}

@end

// Test Shared Tab Groups feature (with group joining access only.).
@interface SharedTabGroupsJoinOnlyTestCase : ChromeTestCase
@end

@implementation SharedTabGroupsJoinOnlyTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabGroupsIPad);
  config.features_enabled.push_back(kModernTabStrip);
  config.features_enabled.push_back(kTabGroupSync);
  config.features_enabled.push_back(
      data_sharing::features::kDataSharingJoinOnly);
  return config;
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

  // Verify that there is no Share button.
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      assertWithMatcher:grey_nil()];
}

@end
