// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/data_sharing/public/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_app_interface.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_eg_utils.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::AlertAction;
using chrome_test_util::CreateTabGroupAtIndex;
using chrome_test_util::LongPressTabGroupCellAtIndex;
using chrome_test_util::RegularTabGrid;
using chrome_test_util::ShareGroupButton;
using chrome_test_util::TabGridGroupCellAtIndex;
using testing::ButtonWithAccessibilityLabel;

namespace {

// Put the number at the beginning to avoid issues with sentence case, as the
// keyboard default can differ iPhone vs iPad, simulator vs device.
NSString* const kGroup1Name = @"1group";

// Matcher for the "Update Chrome" alert button.
id<GREYMatcher> UpdateChromeButton() {
  return AlertAction(l10n_util::GetNSString(
      IDS_COLLABORATION_CHROME_OUT_OF_DATE_ERROR_DIALOG_UPDATE_BUTTON));
}

// Matcher for the "Not now" alert button.
id<GREYMatcher> NotNowButton() {
  return AlertAction(l10n_util::GetNSString(
      IDS_COLLABORATION_CHROME_OUT_OF_DATE_ERROR_DIALOG_NOT_NOW_BUTTON));
}

}  // namespace

// Tests for shared groups when disabled by versioning.
@interface SharedTabGroupsVersioningTestCase : ChromeTestCase
@end

@implementation SharedTabGroupsVersioningTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      data_sharing::features::kDataSharingFeature);
  config.features_enabled.push_back(
      data_sharing::features::kSharedDataTypesKillSwitch);
  config.features_enabled.push_back(
      data_sharing::features::kDataSharingEnableUpdateChromeUI);
  return config;
}

- (void)tearDownHelper {
  [super tearDownHelper];
  // Delete all groups.
  [TabGroupAppInterface cleanup];
}

// Tests that trying to share a group fails with an alert offering to update the
// app.
// TODO(crbug.com/429118547): Test fails on iPhone device.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testSharingPromptsToUpdate DISABLED_testSharingPromptsToUpdate
#else
#define MAYBE_testSharingPromptsToUpdate testSharingPromptsToUpdate
#endif
- (void)MAYBE_testSharingPromptsToUpdate {
  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Create a group with the item at index 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Share the group.
  LongPressTabGroupCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      performAction:grey_tap()];

  // Verify that the Update Chrome option is available.
  [[EarlGrey selectElementWithMatcher:UpdateChromeButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on "Not now".
  [[EarlGrey selectElementWithMatcher:NotNowButton()] performAction:grey_tap()];

  // Verify that the alert is no longer visible.
  [[EarlGrey selectElementWithMatcher:UpdateChromeButton()]
      assertWithMatcher:grey_notVisible()];

  // Verify that the tab grid is visible.
  [[EarlGrey selectElementWithMatcher:RegularTabGrid()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the group is still here.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
