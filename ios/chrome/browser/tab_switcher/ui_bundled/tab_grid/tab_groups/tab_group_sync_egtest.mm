// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "components/data_sharing/public/features.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_app_interface.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_eg_utils.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using chrome_test_util::CancelButton;
using chrome_test_util::CloseGroupButton;
using chrome_test_util::CloseTabGroupButton;
using chrome_test_util::CreateTabGroupAtIndex;
using chrome_test_util::CreateTabGroupCreateButton;
using chrome_test_util::DeleteGroupButton;
using chrome_test_util::DeleteGroupConfirmationButton;
using chrome_test_util::RenameGroupButton;
using chrome_test_util::TabGridCellAtIndex;
using chrome_test_util::TabGridGroupCellAtIndex;
using chrome_test_util::TabGridGroupCellWithName;
using chrome_test_util::TabGridNormalModePageControl;
using chrome_test_util::TabGridOpenTabsPanelButton;
using chrome_test_util::TabGridSearchBar;
using chrome_test_util::TabGridSearchModeToolbar;
using chrome_test_util::TabGridSearchTabsButton;
using chrome_test_util::TabGridTabGroupsPanelButton;
using chrome_test_util::TabGroupCreationView;
using chrome_test_util::TabGroupSnackBar;
using chrome_test_util::TabGroupSnackBarAction;
using chrome_test_util::TabGroupsPanel;
using chrome_test_util::TabGroupsPanelCellAtIndex;
using chrome_test_util::TabGroupsPanelCellWithName;
using chrome_test_util::TabGroupViewTitle;
using chrome_test_util::UngroupButton;
using chrome_test_util::UngroupConfirmationButton;

namespace {

// Put the number at the beginning to avoid issues with sentence case, as the
// keyboard default can differ iPhone vs iPad, simulator vs device.
NSString* const kGroup1Name = @"1group";
NSString* const kGroup2Name = @"2group";

// The groups added by TabGroupAppInterface.
NSString* const kSyncedGroup1Name = @"0RemoteGroup";
NSString* const kSyncedGroup2Name = @"1RemoteGroup";
NSString* const kSyncedGroup3Name = @"2RemoteGroup";

// Displays the group cell context menu by long pressing at the group cell at
// `group_cell_index`.
void DisplayContextMenuForGroupCellAtIndex(int group_cell_index) {
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(group_cell_index)]
      performAction:grey_longPress()];
}

// Renames the group cell at index `group_cell_index` with `new_title`.
void RenameGroupAtIndex(int group_cell_index,
                        NSString* old_title,
                        NSString* new_title) {
  DisplayContextMenuForGroupCellAtIndex(group_cell_index);
  [[EarlGrey selectElementWithMatcher:RenameGroupButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGroupCreationView()];
  for (NSUInteger _ = 0; _ < [old_title length]; _++) {
    // Deletes the old title from the title view.
    [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\b" flags:0];
  }
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:new_title flags:0];
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGroupCreationView()];
}

// Ungroups the group cell at index `group_cell_index`.
void UngroupGroupAtIndex(int group_cell_index) {
  DisplayContextMenuForGroupCellAtIndex(group_cell_index);
  [[EarlGrey selectElementWithMatcher:UngroupButton()]
      performAction:grey_tap()];
  // Tap a ungroup button again to confirm the action.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:UngroupConfirmationButton()];
  [[EarlGrey selectElementWithMatcher:UngroupConfirmationButton()]
      performAction:grey_tap()];

  // Wait until the tab grid cell appears at `group_cell_index`.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:TabGridCellAtIndex(group_cell_index)];
}

// Closes the group cell at index `group_cell_index`.
void CloseGroupAtIndex(int group_cell_index) {
  DisplayContextMenuForGroupCellAtIndex(group_cell_index);
  [[EarlGrey selectElementWithMatcher:CloseGroupButton()]
      performAction:grey_tap()];

  // Wait until the tab group cell disappears at `group_cell_index`.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGridGroupCellAtIndex(
                                                 group_cell_index)];
}

}  // namespace

// Test Tab Group Sync feature.
@interface TabGroupSyncTestCase : ChromeTestCase
@end

@implementation TabGroupSyncTestCase

- (void)tearDownHelper {
  [super tearDownHelper];
  // Delete all saved groups.
  [TabGroupAppInterface cleanup];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  // Add the flag to use FakeTabGroupSyncService.
  config.additional_args.push_back(
      "--" + std::string(test_switches::kEnableFakeTabGroupSyncService));
  config.features_enabled.push_back(
      data_sharing::features::kDataSharingFeature);
  config.features_disabled.push_back(kIOSAutoOpenRemoteTabGroupsSettings);
  return config;
}

// Tests that the third page is the tab groups page.
- (void)testThirdPageIsTabGroups {
  [ChromeEarlGreyUI openTabGrid];

  // Switch over to the tab groups page.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGroupsPanel()]
      assertWithMatcher:grey_notNil()];
}

// Tests that TabGroupAppInterface creates synced tab groups correctly.
- (void)testPreparedSyncedTabGroups {
  // TODO(crbug.com/454868683): Re-enable the test.
  if (@available(iOS 26.1, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.1.");
  }

  GREYAssertEqual(0, [TabGroupAppInterface countOfSavedTabGroups],
                  @"The number of saved tab groups should be 0.");
  [TabGroupAppInterface prepareFakeSyncedTabGroups:3];

  // Sign in to trigger group download.
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                         enableHistorySync:YES];
  [ChromeEarlGrey waitForSyncTransportStateActiveWithTimeout:base::Seconds(10)];

  GREYAssertEqual(3, [TabGroupAppInterface countOfSavedTabGroups],
                  @"The number of saved tab groups should be 3.");

  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Switch over to Tab Groups.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the groups exist.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kSyncedGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kSyncedGroup2Name, 1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kSyncedGroup3Name, 1)]
      assertWithMatcher:grey_notNil()];

  [TabGroupAppInterface cleanup];

  GREYCondition* groupsDeletedCheck = [GREYCondition
      conditionWithName:@"Wait for tab groups to be deleted"
                  block:^{
                    return [TabGroupAppInterface countOfSavedTabGroups] == 0;
                  }];
  bool groupsDeleted = [groupsDeletedCheck waitWithTimeout:10];

  GREYAssertTrue(groupsDeleted, @"Failed to clean up groups");
}

// Tests that a group is deleted in the tab groups page.
- (void)testDeleteTabGroupInTabGroupsPage {
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Switch over to the tab groups page.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group exists.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
  GREYAssertEqual(1, [TabGroupAppInterface countOfSavedTabGroups],
                  @"The number of saved tab groups should be 1.");

  // Delete a group from the context menu of a tab groups panel cell.
  [[EarlGrey selectElementWithMatcher:TabGroupsPanelCellAtIndex(0)]
      performAction:grey_longPress()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:DeleteGroupButton()];
  [[EarlGrey selectElementWithMatcher:DeleteGroupButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:DeleteGroupConfirmationButton()];
  [[EarlGrey selectElementWithMatcher:DeleteGroupConfirmationButton()]
      performAction:grey_tap()];

  // Check that the group is deleted.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGroupsPanelCellWithName(
                                                 kGroup1Name, 1)];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_nil()];

  // Navigate back to the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];

  // Check that the group is deleted in the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGroupViewTitle(kGroup1Name)]
      assertWithMatcher:grey_nil()];

  // Check that the group is deleted from the sync service.
  GREYAssertEqual(0, [TabGroupAppInterface countOfSavedTabGroups],
                  @"The number of saved tab groups should be 0.");
}

// TODO(crbug.com/449704034): Flaky on iphone/ipad simulator.
// Tests that renaming a group in the tab grid reflects the change in the
// Tab Groups panel.
- (void)DISABLED_testRenameGroupInTabGrid {
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Switch over to the tab groups page.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroup1Name` exists but `kGroup2Name` doesn't
  // exist.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup2Name, 1)]
      assertWithMatcher:grey_nil()];

  // Navigate back to the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];

  // Edit the group name from `kGroup1Name` to `kGroup2Name`.
  RenameGroupAtIndex(0, kGroup1Name, kGroup2Name);

  // Switch over to the tab groups page.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroup2Name` exists but `kGroup1Name` doesn't
  // exist.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup2Name, 1)]
      assertWithMatcher:grey_notNil()];
}

// Tests that ungrouping a group in the tab grid reflects the change in the
// Tab Groups panel.
- (void)testUngroupGroupInTabGrid {
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Switch over to the tab groups page.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroup1Name` exists.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
  GREYAssertEqual(1, [TabGroupAppInterface countOfSavedTabGroups],
                  @"The number of saved tab groups should be 1.");

  // Navigate back to the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];

  // Ungroup a group.
  UngroupGroupAtIndex(0);

  // Switch over to the tab groups page.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroup1Name` doesn't exist.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_nil()];

  // Check that the group is deleted from the sync service.
  GREYAssertEqual(0, [TabGroupAppInterface countOfSavedTabGroups],
                  @"The number of saved tab groups should be 0.");
}

// Tests that closing a group in the tab grid reflects the change in the
// Tab Groups panel.
- (void)testCloseGroupInTabGrid {
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Switch over to the tab groups page.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroup1Name` exists.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];

  // Navigate back to the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];

  // Close a group.
  CloseGroupAtIndex(0);

  // Verify the tab group is closed in the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_nil()];

  // Switch over to the tab groups page by tapping the snackbar action.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGroupSnackBar(1)];
  [[EarlGrey selectElementWithMatcher:TabGroupSnackBarAction()]
      performAction:grey_tap()];

  // Check that the group with `kGroup1Name` still exists.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];

  // Check that the group still exists in the sync service.
  GREYAssertEqual(1, [TabGroupAppInterface countOfSavedTabGroups],
                  @"The number of saved tab groups should be 1.");
}

// Tests deleting a saved group from a distant device while the same group is
// being viewed in the tab group view on the current device.
- (void)testDeleteGroupOnAnotherDevice {
  // TODO(crbug.com/454868683): Re-enable the test.
  if (@available(iOS 26.1, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.1.");
  }

  [TabGroupAppInterface prepareFakeSyncedTabGroups:1];

  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                         enableHistorySync:YES];
  [ChromeEarlGrey waitForSyncTransportStateActiveWithTimeout:base::Seconds(10)];

  [ChromeEarlGreyUI openTabGrid];

  // Verify that the group is present in the tab groups panel.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kSyncedGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
  // Navigate back to the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];

  // Open the tab group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(1)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kTabGroupViewIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Delete the group on another device by modifying directly the fake sync
  // server.
  [TabGroupAppInterface removeAtIndex:0];
  GREYCondition* groupsDeletedCheck = [GREYCondition
      conditionWithName:@"Wait for tab group to be deleted"
                  block:^{
                    return [TabGroupAppInterface countOfSavedTabGroups] == 0;
                  }];
  bool groupsDeleted = [groupsDeletedCheck waitWithTimeout:10];

  GREYAssertTrue(groupsDeleted, @"Failed to delete group");

  // The tab group should have been closed automatically.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kTabGroupViewIdentifier)]
      assertWithMatcher:grey_nil()];

  // Verify that the tab group view is not displayed.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(1)]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Verify that the group is also no longer in the tab groups panel.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kSyncedGroup1Name, 1)]
      assertWithMatcher:grey_nil()];
}

// Tests the tab group snackbar CTA.
- (void)testTabGroupSnackbarAction {
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Close a group
  CloseGroupAtIndex(0);

  // Wait for the tab group to disappear and check that the tab disappeared too.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGridGroupCellAtIndex(0)];
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      assertWithMatcher:grey_nil()];

  // Tap on the snackbar action.
  [[EarlGrey selectElementWithMatcher:TabGroupSnackBarAction()]
      performAction:grey_tap()];

  // Check that the tab groups page is shown.
  [[EarlGrey selectElementWithMatcher:TabGroupsPanel()]
      assertWithMatcher:grey_notNil()];

  // Check that the group with `kGroup1Name` still exists.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
}

// Tests that creating a group in the incognito tab grid isn't synced.
- (void)testGroupsNotSyncedInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Switch over to the tab groups page.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroup1Name` doesn't exist.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_nil()];
  GREYAssertEqual(0, [TabGroupAppInterface countOfSavedTabGroups],
                  @"The number of saved tab groups should be 0.");
}

// Tests that the cancellation of ungrouping in the tab grid doesn't ungroup a
// group and ungrouping again after the cancellation works well.
- (void)testConfirmationCancelledForUngroupGroupInTabGrid {
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Try to ungroup a group.
  DisplayContextMenuForGroupCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:UngroupButton()]
      performAction:grey_tap()];

  // Cancel ungrouping a group in the confirmation dialog.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:UngroupConfirmationButton()];

  // No cancel button on iPad and newer iOS; use the identifier of the
  // popover window instead.
  if (iOS26_OR_ABOVE() || [ChromeEarlGrey isIPadIdiom]) {
    [[EarlGrey
        selectElementWithMatcher:GREYAccessibilityID(@"PopoverDismissRegion")]
        performAction:GREYTapAtPoint(CGPointMake(0, 0))];
  } else {
    [[EarlGrey selectElementWithMatcher:CancelButton()]
        performAction:grey_tap()];
  }

  // Wait until the confirmation dialog disappears.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:UngroupConfirmationButton()];

  // Verify that the tab group with `kGroup1Name` still exists.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGridGroupCellWithName(
                                                          kGroup1Name, 1)];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
  GREYAssertEqual(1, [TabGroupAppInterface countOfSavedTabGroups],
                  @"The number of saved tab groups should be 1.");

  // Ungroup a group.
  UngroupGroupAtIndex(0);

  // Verity that the group with `kGroup1Name` doesn't exist anymore.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_nil()];
  GREYAssertEqual(0, [TabGroupAppInterface countOfSavedTabGroups],
                  @"The number of saved tab groups should be 0.");
}

// Tests that the cancellation of deleting in the tab groups page doesn't
// delete a group and deleting a group again after the cancellation works well.
- (void)testConfirmationCancelledForDeleteGroupInTabGroupsPage {
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Switch over to the tab groups page.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group exists.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
  GREYAssertEqual(1, [TabGroupAppInterface countOfSavedTabGroups],
                  @"The number of saved tab groups should be 1.");

  // Try to delete a group from the context menu of a tab groups panel cell.
  [[EarlGrey selectElementWithMatcher:TabGroupsPanelCellAtIndex(0)]
      performAction:grey_longPress()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:DeleteGroupButton()];
  [[EarlGrey selectElementWithMatcher:DeleteGroupButton()]
      performAction:grey_tap()];

  // Cancel the deletion in the confirmation dialog.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:DeleteGroupConfirmationButton()];

  // No cancel button on iPad and newer iOS; use the identifier of the popover
  // window instead.
  if (iOS26_OR_ABOVE() || [ChromeEarlGrey isIPadIdiom]) {
    [[EarlGrey
        selectElementWithMatcher:GREYAccessibilityID(@"PopoverDismissRegion")]
        performAction:GREYTapAtPoint(CGPointMake(0, 0))];
  } else {
    [[EarlGrey selectElementWithMatcher:CancelButton()]
        performAction:grey_tap()];
  }

  // Wait until the confirmation dialog disappears.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:DeleteGroupConfirmationButton()];

  // Verify that the group with `kGroup1Name` still exists.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:TabGroupsPanelCellWithName(
                                              kGroup1Name, 1)];
  GREYAssertEqual(1, [TabGroupAppInterface countOfSavedTabGroups],
                  @"The number of saved tab groups should be 1.");

  // Delete a group from the context menu of a tab groups panel cell.
  [[EarlGrey selectElementWithMatcher:TabGroupsPanelCellAtIndex(0)]
      performAction:grey_longPress()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:DeleteGroupButton()];
  [[EarlGrey selectElementWithMatcher:DeleteGroupButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:DeleteGroupConfirmationButton()];
  [[EarlGrey selectElementWithMatcher:DeleteGroupConfirmationButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroup1Name` is deleted.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGroupsPanelCellWithName(
                                                 kGroup1Name, 1)];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_nil()];
  GREYAssertEqual(0, [TabGroupAppInterface countOfSavedTabGroups],
                  @"The number of saved tab groups should be 0.");
}

// Tests that Search mode is exited when focusing the Tab Groups panel via the
// snackbar that appears after closing a group from Search results.
- (void)testSearchModeExitsWhenOpeningTabGroupsPanelFromSnackbar {
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Verify that search mode is active.
  [[EarlGrey selectElementWithMatcher:TabGridSearchModeToolbar()]
      assertWithMatcher:grey_notNil()];

  // Search for the group.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(kGroup1Name)];

  // Close the group from the search results.
  CloseGroupAtIndex(0);

  // Verify the tab group is closed in the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_nil()];

  // Switch over to the tab groups page by tapping the snackbar action.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGroupSnackBar(1)];
  [[EarlGrey selectElementWithMatcher:TabGroupSnackBarAction()]
      performAction:grey_tap()];

  // Check that the group with `kGroup1Name` still exists.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];

  // Verify that normal mode is active.
  [[EarlGrey selectElementWithMatcher:TabGridNormalModePageControl()]
      assertWithMatcher:grey_notNil()];
}

@end
