// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/public/group_data.h"
#import "components/data_sharing/test_support/test_utils.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_app_interface.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/test/query_title_server_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::CreateTabGroupTextField;
using chrome_test_util::DeleteSharedConfirmationButton;
using chrome_test_util::DeleteSharedGroupButton;
using chrome_test_util::FakeJoinFlowView;
using chrome_test_util::FakeShareFlowView;
using chrome_test_util::LeaveSharedGroupButton;
using chrome_test_util::LeaveSharedGroupConfirmationButton;
using chrome_test_util::NavigationBarSaveButton;
using chrome_test_util::ShareGroupButton;
using chrome_test_util::UngroupButton;

namespace {

NSString* const kGroupTitle1 = @"Group Title 1";

// Returns a matcher for the currently selected tab strip tab cell.
id<GREYMatcher> TabStripTabCellSelectedMatcher() {
  return grey_allOf(grey_kindOfClassName(@"UIView"),
                    grey_not(grey_kindOfClassName(@"UILabel")),
                    grey_accessibilityTrait(UIAccessibilityTraitSelected),
                    grey_ancestor(grey_kindOfClassName(@"TabStripTabCell")),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for a tab strip group cell with `title` as title.
id<GREYMatcher> TabStripGroupCellMatcher(NSString* title) {
  return grey_allOf(grey_kindOfClassName(@"UIView"),
                    grey_not(grey_kindOfClassName(@"UILabel")),
                    grey_accessibilityLabel(title),
                    grey_ancestor(grey_kindOfClassName(@"TabStripGroupCell")),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for a context menu button with an accessibility label
// matching `accessibility_label_id`.
id<GREYMatcher> ContextMenuButtonMatcher(int accessibility_label_id) {
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabelId(accessibility_label_id),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

// Returns a matcher for a context menu button with an accessibility label
// matching `accessibility_label_id` and `accessibility_label_number` (in case
// the accessibility label string depends on some integer input).
id<GREYMatcher> ContextMenuButtonMatcher(int accessibility_label_id,
                                         int accessibility_label_number) {
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabelIdAndNumberForPlural(
          accessibility_label_id, accessibility_label_number),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

// Adds the tab matching `tab_cell_matcher` to a new group with title
// `group_title`.
void AddTabToNewGroup(id<GREYMatcher> tab_cell_matcher,
                      NSString* group_title,
                      bool sub_menu = false) {
  [[EarlGrey selectElementWithMatcher:tab_cell_matcher]
      performAction:grey_longPress()];
  if (sub_menu) {
    [[EarlGrey
        selectElementWithMatcher:ContextMenuButtonMatcher(
                                     IDS_IOS_CONTENT_CONTEXT_ADDTABTOTABGROUP,
                                     1)] performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:
                   ContextMenuButtonMatcher(
                       IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP_SUBMENU)]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:
                   ContextMenuButtonMatcher(
                       IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP, 1)]
        performAction:grey_tap()];
  }
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              kCreateTabGroupViewIdentifier)];
  [[EarlGrey selectElementWithMatcher:CreateTabGroupTextField()]
      performAction:grey_replaceText(group_title)];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(kCreateTabGroupViewIdentifier)];
}

}  // namespace

// Tests for shared tab groups on the tab strip shown on iPad.
@interface TabStripSharedGroupTestCase : ChromeTestCase
@end

@implementation TabStripSharedGroupTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabGroupsIPad);
  config.features_enabled.push_back(kModernTabStrip);
  config.features_enabled.push_back(kTabGroupSync);
  config.features_enabled.push_back(
      data_sharing::features::kDataSharingFeature);
  // Add the flag to use FakeTabGroupSyncService.
  config.additional_args.push_back(
      "--" + std::string(test_switches::kEnableFakeTabGroupSyncService));
  return config;
}

- (void)setUp {
  [super setUp];
  RegisterQueryTitleHandler(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");
  [ChromeEarlGrey
      setUserDefaultsObject:@YES
                     forKey:kSharedTabGroupUserEducationShownOnceKey];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                         enableHistorySync:YES];
}

// Tests that deleting a shared tab group from tab strip works.
- (void)testTabStripSharedGroupDeleteSharedGroup {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  // Add the current tab to a new group.
  AddTabToNewGroup(TabStripTabCellSelectedMatcher(), kGroupTitle1);

  // Share the group.
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle1)]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:FakeShareFlowView()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:NavigationBarSaveButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:FakeShareFlowView()]
      assertWithMatcher:grey_notVisible()];

  // Long press the group.
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle1)]
      performAction:grey_longPress()];

  // Verify that the leave and ungroup buttons are not available.
  [[EarlGrey selectElementWithMatcher:LeaveSharedGroupButton()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:UngroupButton()]
      assertWithMatcher:grey_notVisible()];

  // Delete the shared group.
  [[EarlGrey selectElementWithMatcher:DeleteSharedGroupButton()]
      performAction:grey_tap()];
  // Tap on the delete button again to confirm the deletion.
  [[EarlGrey selectElementWithMatcher:DeleteSharedConfirmationButton()]
      performAction:grey_tap()];

  // Check that the group has been deleted.
  GREYCondition* groupsDeletedCheck =
      [GREYCondition conditionWithName:@"Wait for tab groups to be deleted"
                                 block:^{
                                   return [ChromeEarlGrey mainTabCount] == 0;
                                 }];
  bool groupsDeleted = [groupsDeletedCheck waitWithTimeout:10];
  GREYAssertTrue(groupsDeleted, @"Failed to delete the shared group");
}

// Tests that leaving a shared tab group from tab strip works.
- (void)testTabStripSharedGroupLeaveSharedGroup {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  [TabGroupAppInterface mockSharedEntitiesPreview];
  GURL joinGroupURL = data_sharing::GetDataSharingUrl(data_sharing::GroupToken(
      data_sharing::GroupId("resources%2F3be"), "CggHBicxA_slvx"));
  [ChromeEarlGrey loadURL:joinGroupURL waitForCompletion:NO];

  // Verify that it opened the Join flow.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:FakeJoinFlowView()];

  // Join the group.
  [[EarlGrey selectElementWithMatcher:NavigationBarSaveButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:FakeJoinFlowView()]
      assertWithMatcher:grey_notVisible()];

  // Long press the group.
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(@"shared group")]
      performAction:grey_longPress()];

  // Verify that the delete and ungroup buttons are not available.
  [[EarlGrey selectElementWithMatcher:DeleteSharedGroupButton()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:UngroupButton()]
      assertWithMatcher:grey_notVisible()];

  // Leave the shared group.
  [[EarlGrey selectElementWithMatcher:LeaveSharedGroupButton()]
      performAction:grey_tap()];
  // Tap on the leave button confirmation.
  [[EarlGrey selectElementWithMatcher:LeaveSharedGroupConfirmationButton()]
      performAction:grey_tap()];

  // Check that the group has been leaved.
  GREYCondition* groupsLeavedCheck =
      [GREYCondition conditionWithName:@"Wait for tab groups to be leaved"
                                 block:^{
                                   return [ChromeEarlGrey mainTabCount] == 1;
                                 }];
  bool groupsLeaved = [groupsLeavedCheck waitWithTimeout:10];
  GREYAssertTrue(groupsLeaved, @"Failed to leave the shared group");
}

@end
