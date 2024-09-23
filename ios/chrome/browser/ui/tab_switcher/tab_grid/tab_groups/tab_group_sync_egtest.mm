// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_sync_earl_grey.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_eg_utils.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using chrome_test_util::CloseGroupButton;
using chrome_test_util::CreateTabGroupAtIndex;
using chrome_test_util::CreateTabGroupCreateButton;
using chrome_test_util::DeleteGroupButton;
using chrome_test_util::DeleteGroupConfirmationButton;
using chrome_test_util::RenameGroupButton;
using chrome_test_util::TabGridCellAtIndex;
using chrome_test_util::TabGridGroupCellAtIndex;
using chrome_test_util::TabGridGroupCellWithName;
using chrome_test_util::TabGridOpenTabsPanelButton;
using chrome_test_util::TabGridTabGroupsPanelButton;
using chrome_test_util::TabGroupBackButton;
using chrome_test_util::TabGroupCreationView;
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

// The groups added by FakeTabGroupSyncService::PrepareFakeSavedTabGroups().
NSString* const kSavedGroup1Name = @"1RemoteGroup";
NSString* const kSavedGroup2Name = @"2RemoteGroup";
NSString* const kSavedGroup3Name = @"3RemoteGroup";

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

  // Waits until the tab grid cell appears at `group_cell_index`.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:TabGridCellAtIndex(group_cell_index)];
}

// Closes the group cell at index `group_cell_index`.
void CloseGroupAtIndex(int group_cell_index) {
  DisplayContextMenuForGroupCellAtIndex(group_cell_index);
  [[EarlGrey selectElementWithMatcher:CloseGroupButton()]
      performAction:grey_tap()];

  // Waits until the tab group cell disappears at `group_cell_index`.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGridGroupCellAtIndex(
                                                 group_cell_index)];
}

}  // namespace

// Test Tab Group Sync feature.
@interface TabGroupSyncTestCase : ChromeTestCase
@end

@implementation TabGroupSyncTestCase

- (void)tearDown {
  [super tearDown];
  // Delete all saved groups.
  [TabGroupSyncEarlGrey cleanup];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabGroupsIPad);
  config.features_enabled.push_back(kModernTabStrip);
  config.features_enabled.push_back(kTabGroupSync);

  // Add the flag to use FakeTabGroupSyncService.
  config.additional_args.push_back(
      "--" + std::string(test_switches::kEnableFakeTabGroupSyncService));
  return config;
}

// Tests that the third panel is Tab Groups panel.
- (void)testThirdPanelIsTabGroups {
  [ChromeEarlGreyUI openTabGrid];

  // Switch over to the third panel.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGroupsPanel()]
      assertWithMatcher:grey_notNil()];
}

// Tests that TabGroupSyncEarlGrey creates saved tab groups correctly.
- (void)testPreparedSavedTabGroups {
  GREYAssertEqual(0, [TabGroupSyncEarlGrey countOfSavedTabGroups],
                  @"The number of saved tab groups should be 0.");
  [TabGroupSyncEarlGrey prepareFakeSavedTabGroups];
  GREYAssertEqual(3, [TabGroupSyncEarlGrey countOfSavedTabGroups],
                  @"The number of saved tab groups should be 3.");

  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Switch over to Tab Groups.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the groups exist.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kSavedGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kSavedGroup2Name, 1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kSavedGroup3Name, 1)]
      assertWithMatcher:grey_notNil()];

  [TabGroupSyncEarlGrey cleanup];
  GREYAssertEqual(0, [TabGroupSyncEarlGrey countOfSavedTabGroups],
                  @"The number of saved tab groups should be 0.");
}

// Tests that a group is deleted in the Tab Groups panel.
- (void)testDeleteTabGroupInThirdPanel {
  [ChromeEarlGreyUI openTabGrid];

  // Creates a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Switch over to the third panel.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group exists.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
  GREYAssertEqual(1, [TabGroupSyncEarlGrey countOfSavedTabGroups],
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
  GREYAssertEqual(0, [TabGroupSyncEarlGrey countOfSavedTabGroups],
                  @"The number of saved tab groups should be 0.");
}

// Tests that renaming a group in the tab grid reflects the change in the
// Tab Groups panel.
- (void)testRenameGroupInTabGrid {
  [ChromeEarlGreyUI openTabGrid];

  // Creates a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Switch over to the third panel.
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

  // Switch over to the third panel.
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

  // Creates a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Switch over to the third panel.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroup1Name` exists.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
  GREYAssertEqual(1, [TabGroupSyncEarlGrey countOfSavedTabGroups],
                  @"The number of saved tab groups should be 1.");

  // Navigate back to the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];

  // Ungroup a group.
  UngroupGroupAtIndex(0);

  // Switch over to the third panel.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroup1Name` doesn't exist.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_nil()];

  // Check that the group is deleted from the sync service.
  GREYAssertEqual(0, [TabGroupSyncEarlGrey countOfSavedTabGroups],
                  @"The number of saved tab groups should be 0.");
}

// Tests that closing a group in the tab grid reflects the change in the
// Tab Groups panel.
- (void)testCloseGroupInTabGrid {
  [ChromeEarlGreyUI openTabGrid];

  // Creates a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Switch over to the third panel.
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

  // Switch over to the third panel.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:TabGridTabGroupsPanelButton()];
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroup1Name` still exists.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];

  // Check that the group still exists in the sync service.
  GREYAssertEqual(1, [TabGroupSyncEarlGrey countOfSavedTabGroups],
                  @"The number of saved tab groups should be 1.");
}

// Tests deleting a saved group from one device while the same group is
// being viewed in the tab group view on a different device.
- (void)testDeleteGroupOnAnotherDevice {
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Open the tab group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Verify that the tab group view is displayed.
  [[EarlGrey selectElementWithMatcher:TabGroupViewTitle(kGroup1Name)]
      assertWithMatcher:grey_notNil()];

  // Delete the group on another device by modifying directly
  // TabGroupSyncService.
  [TabGroupSyncEarlGrey removeAtIndex:0];
  GREYAssertEqual(0, [TabGroupSyncEarlGrey countOfSavedTabGroups],
                  @"The number of saved tab groups should be 0.");

  // Go back to the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGroupBackButton()]
      performAction:grey_tap()];

  // Verify that the tab group view is not displayed.
  [[EarlGrey selectElementWithMatcher:TabGroupViewTitle(kGroup1Name)]
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

  // Check that the Tab Groups Panel is shown.
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

  // Creates a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Switch over to the third panel.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroup1Name` doesn't exist.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_nil()];
  GREYAssertEqual(0, [TabGroupSyncEarlGrey countOfSavedTabGroups],
                  @"The number of saved tab groups should be 0.");
}

@end
