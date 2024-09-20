// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/test/query_title_server_util.h"
#import "ios/chrome/browser/ui/tab_switcher/test/tabs_egtest_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::AddTabToGroupSubMenuButton;
using chrome_test_util::AddTabToNewGroupButton;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::CloseGroupButton;
using chrome_test_util::ContextMenuItemWithAccessibilityLabel;
using chrome_test_util::ContextMenuItemWithAccessibilityLabelId;
using chrome_test_util::CreateTabGroupCancelButton;
using chrome_test_util::CreateTabGroupCreateButton;
using chrome_test_util::CreateTabGroupTextField;
using chrome_test_util::CreateTabGroupTextFieldClearButton;
using chrome_test_util::DeleteGroupButton;
using chrome_test_util::DeleteGroupConfirmationButton;
using chrome_test_util::RenameGroupButton;
using chrome_test_util::TabGridCellAtIndex;
using chrome_test_util::TabGridEditAddToButton;
using chrome_test_util::TabGridEditButton;
using chrome_test_util::TabGridEditMenuCloseAllButton;
using chrome_test_util::TabGridGroupCellAtIndex;
using chrome_test_util::TabGridGroupCellWithName;
using chrome_test_util::TabGridNewTabButton;
using chrome_test_util::TabGridSearchBar;
using chrome_test_util::TabGridSearchTabsButton;
using chrome_test_util::TabGridSelectTabsMenuButton;
using chrome_test_util::TabGridUndoCloseAllButton;
using chrome_test_util::TabGroupBackButton;
using chrome_test_util::TabGroupCreationView;
using chrome_test_util::TabGroupOverflowMenuButton;
using chrome_test_util::TabGroupSnackBar;
using chrome_test_util::TabGroupView;
using chrome_test_util::TabGroupViewTitle;
using chrome_test_util::UngroupButton;
using chrome_test_util::UngroupConfirmationButton;
using chrome_test_util::WindowWithNumber;
using testing::NavigationBarBackButton;

namespace {

NSString* const kTab1Title = @"Tab1";
NSString* const kTab2Title = @"Tab2";

// Put the number at the beginning to avoid issues with sentence case, as the
// keyboard default can differ iPhone vs iPad, simulator vs device.
NSString* const kGroup1Name = @"1group";
NSString* const kGroup2Name = @"2group";

// Displays the tab cell context menu by long pressing at the tab cell at
// `tab_cell_index`.
void DisplayContextMenuForTabCellAtIndex(int tab_cell_index) {
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(tab_cell_index)]
      performAction:grey_longPress()];
}

// Displays the group cell context menu by long pressing at the group cell at
// `group_cell_index`.
void DisplayContextMenuForGroupCellAtIndex(int group_cell_index) {
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(group_cell_index)]
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

// Creates a group with default title from a tab cell at index `tab_cell_index`
// when the grid contains groups.
void CreateAdditionalDefaultGroupFromTabCellAtIndex(int tab_cell_index) {
  DisplayContextMenuForTabCellAtIndex(tab_cell_index);
  [[EarlGrey
      selectElementWithMatcher:ContextMenuItemWithAccessibilityLabel(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_CONTENT_CONTEXT_ADDTABTOTABGROUP,
                                       1))] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AddTabToGroupSubMenuButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGroupCreationView()];
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButton()]
      performAction:grey_tap()];
}

// Returns the matcher for the tab grid tab count.
id<GREYMatcher> TabGridTabCount(NSString* count_string) {
  return grey_allOf(grey_ancestor(grey_kindOfClassName(@"TabGridPageControl")),
                    grey_text(count_string), grey_sufficientlyVisible(), nil);
}

// Adds the tab at `tab_cell_index` to the group with `title`.
void AddTabAtIndexToGroupWithTitle(int tab_cell_index, NSString* title) {
  DisplayContextMenuForTabCellAtIndex(tab_cell_index);
  [[EarlGrey
      selectElementWithMatcher:ContextMenuItemWithAccessibilityLabel(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_CONTENT_CONTEXT_ADDTABTOTABGROUP,
                                       1))] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabel(
                                          title)] performAction:grey_tap()];
}

// Opens the tab group at `group_cell_index`.
void OpenTabGroupAtIndex(int group_cell_index) {
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGridGroupCellAtIndex(
                                                          group_cell_index)];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(group_cell_index)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGridCellAtIndex(0)];
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

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGroupCreationView()];
}

// Sets the tab group name in the tab group creation view.
void SetTabGroupCreationName(NSString* group_name) {
  [[EarlGrey selectElementWithMatcher:CreateTabGroupTextField()]
      performAction:grey_tap()];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:group_name flags:0];
}

// Renames the group cell at index `group_cell_index` with `title`.
void RenameGroupAtIndex(int group_cell_index, NSString* title) {
  DisplayContextMenuForGroupCellAtIndex(group_cell_index);
  [[EarlGrey selectElementWithMatcher:RenameGroupButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGroupCreationView()];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:title flags:0];
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
  // Tap a ungroup button again to confirm the deletion.
  [[EarlGrey selectElementWithMatcher:UngroupConfirmationButton()]
      performAction:grey_tap()];
}

// Deletes the group cell at index `group_cell_index`.
void DeleteGroupAtIndex(int group_cell_index) {
  DisplayContextMenuForGroupCellAtIndex(group_cell_index);
  [[EarlGrey selectElementWithMatcher:DeleteGroupButton()]
      performAction:grey_tap()];
  // Tap a delete button again to confirm the deletion.
  [[EarlGrey selectElementWithMatcher:DeleteGroupConfirmationButton()]
      performAction:grey_tap()];
}

// Closes the group cell at index `group_cell_index`.
void CloseGroupAtIndex(int group_cell_index) {
  DisplayContextMenuForGroupCellAtIndex(group_cell_index);
  [[EarlGrey selectElementWithMatcher:CloseGroupButton()]
      performAction:grey_tap()];
}

// Matcher for the pinned cell at the given `index`.
id<GREYMatcher> GetMatcherForPinnedCellWithTitle(NSString* title) {
  return grey_allOf(
      grey_accessibilityLabel([NSString stringWithFormat:@"Pinned, %@", title]),
      grey_kindOfClassName(@"PinnedCell"), grey_sufficientlyVisible(), nil);
}

}  // namespace

// Test Tab Groups feature.
@interface TabGroupsTestCase : ChromeTestCase
@end

@implementation TabGroupsTestCase

- (void)setUp {
  [super setUp];
  RegisterQueryTitleHandler(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kIncognitoAuthenticationSetting];
}

- (void)tearDown {
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kIncognitoAuthenticationSetting];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabGroupsIPad);
  config.features_enabled.push_back(kModernTabStrip);
  config.features_enabled.push_back(kTabGroupIndicator);
  if ([self isRunningTest:@selector(testCloseFromSelectionSyncDisabled)]) {
    config.features_disabled.push_back(kTabGroupSync);
  } else {
    config.features_enabled.push_back(kTabGroupSync);
  }
  return config;
}

// Verifies that the tab grid has exactly `expectedCount` tabs.
- (void)verifyVisibleTabsCount:(NSUInteger)expectedCount {
  // Verify that the cell # `expectedCount` exist.
  if (expectedCount == 0) {
    [[EarlGrey selectElementWithMatcher:TabGridCell()]
        assertWithMatcher:grey_nil()];
  } else {
    [[[EarlGrey selectElementWithMatcher:TabGridCell()]
        atIndex:expectedCount - 1] assertWithMatcher:grey_notNil()];
  }
  // Then verify that there is no more cells after that.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(TabGridCell(),
                                          TabGridCellAtIndex(expectedCount),
                                          nil)] assertWithMatcher:grey_nil()];
}

// Tests that creates a tab group and opens the grouped tab.
- (void)testCompleteTabGroupCreation {
  [ChromeEarlGreyUI openTabGrid];

  // Open the creation view.
  OpenTabGroupCreationViewUsingLongPressForCellAtIndex(0);
  SetTabGroupCreationName(kGroup1Name);

  // Valid the creation.
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGroupCreationView()];

  // Open the group.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(kGroup1Name, 1)]
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
  SetTabGroupCreationName(kGroup1Name);

  // Cancel the creation.
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCancelButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGroupCreationView()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(kGroup1Name, 0)]
      assertWithMatcher:grey_nil()];
}

// Tests clearing the text field in Tab Group creation flow.
- (void)testClearTextTabGroupCreation {
  [ChromeEarlGreyUI openTabGrid];

  // Open the creation view.
  OpenTabGroupCreationViewUsingLongPressForCellAtIndex(0);
  SetTabGroupCreationName(kGroup1Name);

  // Clear the text field.
  [[EarlGrey selectElementWithMatcher:CreateTabGroupTextFieldClearButton()]
      performAction:grey_tap()];

  // Valid the creation.
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButton()]
      performAction:grey_tap()];

  // Check that there is no group named kGroup1Name but one with the number of
  // tabs.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGroupCreationView()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(kGroup1Name, 0)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 1),
                                          1)] assertWithMatcher:grey_notNil()];
}

// Tests the group creation based on the context menu a tab cell in the grid.
- (void)testGroupCreationUsingTabContextMenuInGrid {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  // Check for the presence of the tab cell with the title `Tab 1` in the grid.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_notNil()];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  // `Tab 1` tab cell no longer present in the grid.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_nil()];

  OpenTabGroupAtIndex(0);

  // Check that `Tab 1` tab cell is in the group.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:NavigationBarBackButton()]
      performAction:grey_tap()];

  // Create a tab cell with `Tab 2` as its title.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab2Title)];
  [ChromeEarlGreyUI openTabGrid];

  CreateAdditionalDefaultGroupFromTabCellAtIndex(1);

  // `Tab 2` tab cell no longer present in the grid.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab2Title)]
      assertWithMatcher:grey_nil()];

  OpenTabGroupAtIndex(1);

  // Check that `Tab 2` tab cell is in the group.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab2Title)]
      assertWithMatcher:grey_notNil()];
}

// Tests adding a tab to a group from the tab's context menu.
- (void)testAddingTabToGroupUsingTabContextMenu {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  // Check for the presence of the tab cell with the title `Tab 1` in the grid.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TabGridTabCount(@"1")]
      assertWithMatcher:grey_notNil()];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  // `Tab 1` tab cell no longer present in the grid.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_nil()];

  // Create a tab cell with `Tab 2` as its title.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab2Title)];
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridTabCount(@"2")]
      assertWithMatcher:grey_notNil()];

  AddTabAtIndexToGroupWithTitle(
      1, l10n_util::GetPluralNSStringF(IDS_IOS_TAB_GROUP_TABS_NUMBER, 1));

  // `Tab 2` tab cell no longer present in the grid.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab2Title)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:TabGridTabCount(@"2")]
      assertWithMatcher:grey_notNil()];

  OpenTabGroupAtIndex(0);

  // Check that `Tab 2` tab cell is in the group.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab2Title)]
      assertWithMatcher:grey_notNil()];

  // Close the tab and check it is no longer visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(1)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab2Title)]
      assertWithMatcher:grey_nil()];
}

// Tests the group renaming from the group's context menu in the grid.
- (void)testRenamingGroupUsingGridContextMenu {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  // Check for the presence of the tab cell with the title `Tab 1` in the grid.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_notNil()];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  // `Tab 1` tab cell no longer present in the grid.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_nil()];

  RenameGroupAtIndex(0, kGroup1Name);

  // Check the group's new name.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
}

// Tests the ungrouping of a group from its context menu in the grid.
- (void)testUngroupingGroupUsingGridContextMenu {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  UngroupGroupAtIndex(0);

  // `Tab 1` tab cell is now present in the grid.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabWithTitle(kTab1Title)];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_notNil()];

  // The created group is no longer in the grid.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 1),
                                          1)] assertWithMatcher:grey_nil()];
}

// Tests the group deletion from the group's context menu in the grid.
- (void)testDeletingGroupUsingGridContextMenu {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  DeleteGroupAtIndex(0);

  // The tab and the group are deleted.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 1),
                                          1)] assertWithMatcher:grey_nil()];

  // Check that the snackbar is not displayed.
  [[EarlGrey selectElementWithMatcher:TabGroupSnackBar(1)]
      assertWithMatcher:grey_nil()];
}

// Tests closing a group from the group's context menu action in the grid.
- (void)testClosingGroupUsingGridContextMenu {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  CloseGroupAtIndex(0);

  // The tab and the group are closed.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 1),
                                          1)] assertWithMatcher:grey_nil()];

  // Check that the snackbar is displayed.
  [[EarlGrey selectElementWithMatcher:TabGroupSnackBar(1)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests ungrouping of a group from the overflow menu in the group view.
- (void)testUngroupingGroupFromGroupView {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  // Open the group view.
  OpenTabGroupAtIndex(0);

  // Display the tab group overflow menu.
  [[EarlGrey selectElementWithMatcher:TabGroupOverflowMenuButton()]
      performAction:grey_tap()];

  // Tap the ungroup button.
  [[EarlGrey selectElementWithMatcher:UngroupButton()]
      performAction:grey_tap()];
  // Tap a delete button again to confirm the deletion.
  [[EarlGrey selectElementWithMatcher:UngroupConfirmationButton()]
      performAction:grey_tap()];

  // `Tab 1` tab cell is now present in the grid.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabWithTitle(kTab1Title)];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_notNil()];

  // The created group is no longer in the grid.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 1),
                                          1)] assertWithMatcher:grey_nil()];
}

// Tests the group deletion from the overflow menu in the group view.
- (void)testDeletingGroupFromGroupView {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  // Open the group view.
  OpenTabGroupAtIndex(0);

  // Display the tab group overflow menu.
  [[EarlGrey selectElementWithMatcher:TabGroupOverflowMenuButton()]
      performAction:grey_tap()];

  // Tap the delete button.
  [[EarlGrey selectElementWithMatcher:DeleteGroupButton()]
      performAction:grey_tap()];
  // Tap a delete button again to confirm the deletion.
  [[EarlGrey selectElementWithMatcher:DeleteGroupConfirmationButton()]
      performAction:grey_tap()];

  // The tab and the group are deleted.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 1),
                                          1)] assertWithMatcher:grey_nil()];
}

// Tests tapping on the "+" button in the Tab Group view.
- (void)testAddNewTabButtonFromGroupView {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  [ChromeEarlGrey waitForMainTabCount:1];

  // Open the group view.
  OpenTabGroupAtIndex(0);

  // Check that the title is "1 tab".
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTabGroupViewTitleIdentifier)]
      assertWithMatcher:grey_accessibilityLabel(l10n_util::GetPluralNSStringF(
                            IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(ButtonWithAccessibilityLabelId(
                                              IDS_IOS_TAB_GRID_CREATE_NEW_TAB),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForMainTabCount:2];

  [ChromeEarlGreyUI openTabGrid];

  // Make sure the Tab Group view is reopened and its title is "2 tabs".
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTabGroupViewTitleIdentifier)]
      assertWithMatcher:grey_accessibilityLabel(l10n_util::GetPluralNSStringF(
                            IDS_IOS_TAB_GROUP_TABS_NUMBER, 2))];
}

// Tests cancelling of the deletion of a group from the overflow menu in the
// group view.
- (void)testCancellingActionToGroupFromGroupView {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  // Open the group view.
  OpenTabGroupAtIndex(0);

  // Display the tab group overflow menu.
  [[EarlGrey selectElementWithMatcher:TabGroupOverflowMenuButton()]
      performAction:grey_tap()];

  // Tap the delete button.
  [[EarlGrey selectElementWithMatcher:DeleteGroupButton()]
      performAction:grey_tap()];
  // Cancel the action by tapping a tab itself (= outside the delete button).
  // We have a cancel button only on iPhone.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      performAction:grey_tap()];

  // Check that `Tab 1` tab cell still exists in the group.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_notNil()];

  // Go back to the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGroupBackButton()]
      performAction:grey_tap()];
  // Check that the group still exists.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 1),
                                          1)] assertWithMatcher:grey_notNil()];
}

// Tests closing the group from the close button.
- (void)testCloseTabGroup {
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.iph_feature_enabled = "IPH_iOSSavedTabGroupClosed";
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::TabGridCloseButtonForGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // The tab and the group are deleted.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 1),
                                          1)] assertWithMatcher:grey_nil()];

  // The snackbar is not shown, the IPH is shown.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_TAB_GRID_SAVED_TAB_GROUPS)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:TabGroupSnackBar(1)]
      assertWithMatcher:grey_nil()];

  // Check that the second close doesn't trigger the IPH.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];
  CreateDefaultFirstGroupFromTabCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::TabGridCloseButtonForGroupCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 1),
                                          1)] assertWithMatcher:grey_nil()];

  // The snackbar is shown, the IPH is not shown.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_TAB_GRID_SAVED_TAB_GROUPS)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:TabGroupSnackBar(1)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the creation of a new group in selection mode.
- (void)testGroupCreationInSelectionMode {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  // Enter the selection mode.
  [[EarlGrey selectElementWithMatcher:TabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];

  // Select the cell with the title `kTab1Title`.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      performAction:grey_tap()];

  // Add the tab to a new group.
  [[EarlGrey selectElementWithMatcher:TabGridEditAddToButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AddTabToNewGroupButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGroupCreationView()];
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGroupCreationView()];

  // `Tab 1` tab cell no longer present in the grid.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_nil()];

  // The group with title `1 Tab` is present in the grid.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 1),
                                          1)] assertWithMatcher:grey_notNil()];
}

// Tests the adding a tab to a group in selection mode.
- (void)testAddingTabToGroupInSelectionMode {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  // Create a tab cell with `Tab 2` as its title.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab2Title)];
  [ChromeEarlGreyUI openTabGrid];

  // Enter the selection mode.
  [[EarlGrey selectElementWithMatcher:TabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];

  // Select the cell with the title `kTab2Title`.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(1)]
      performAction:grey_tap()];

  // Add the selected tab to the group with title `1 Tab`.
  [[EarlGrey selectElementWithMatcher:TabGridEditAddToButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ContextMenuItemWithAccessibilityLabel(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_CONTENT_CONTEXT_ADDTABTOTABGROUP,
                                       1))] performAction:grey_tap()];
  NSString* title =
      l10n_util::GetPluralNSStringF(IDS_IOS_TAB_GROUP_TABS_NUMBER, 1);
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   ContextMenuItemWithAccessibilityLabel(title),
                                   grey_not(grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled)),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // `Tab 2` tab cell no longer present in the grid.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab2Title)]
      assertWithMatcher:grey_nil()];

  // The group with title `1 Tab` is no longer present in the grid.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 1),
                                          1)] assertWithMatcher:grey_nil()];

  // The group with title `2 Tabs` is present in the grid.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 2),
                                          2)] assertWithMatcher:grey_notNil()];
}

// Checks that all the options are displayed in the group's overflow menu.
- (void)testAppropriateOverflowMenuInGroupView {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  OpenTabGroupAtIndex(0);

  // Display the tab group overflow menu.
  [[EarlGrey selectElementWithMatcher:TabGroupOverflowMenuButton()]
      performAction:grey_tap()];

  // Check the different buttons.
  [[EarlGrey selectElementWithMatcher:RenameGroupButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:UngroupButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DeleteGroupButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:CloseGroupButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests closing all tabs and groups in grid, and that the closing is reversible
// when pressing the undo button.
- (void)testCloseAllAndUndo {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  // Create a tab cell with `Tab 2` as its title.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab2Title)];
  [ChromeEarlGreyUI openTabGrid];

  // Check that `Tab 2` and the group with title ` 1 Tab`are in the grid and
  // `Tab 1` is not.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab2Title)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 1),
                                          1)] assertWithMatcher:grey_notNil()];

  // Close all (groups and tabs).
  [[EarlGrey selectElementWithMatcher:TabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridEditMenuCloseAllButton()]
      performAction:grey_tap()];

  // Check that `Tab 2` and the group with title `1 Tab` are no longer in the
  // grid.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab2Title)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 1),
                                          1)] assertWithMatcher:grey_nil()];

  // Check that the snackbar is displayed.
  [[EarlGrey selectElementWithMatcher:TabGroupSnackBar(1)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Undo button.
  [[EarlGrey selectElementWithMatcher:TabGridUndoCloseAllButton()]
      performAction:grey_tap()];

  // Check that `Tab 2` and the group with title `1 Tab` are back in the grid.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab2Title)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 1),
                                          1)] assertWithMatcher:grey_notNil()];
}

// Tests opening a tab group after resetting the incognito browser (i.e. closing
// all incognito tabs).
- (void)testOpenTabGroupAfterBrowserReset {
  // Create one incognito tab then close all of them to reset the browser.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:1];
  [ChromeEarlGrey closeAllIncognitoTabs];
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  OpenTabGroupAtIndex(0);
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];
}

// Tests re-opening a group from Search in another window.
- (void)testReopenGroupFromAnotherWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");
  }

  // Create a first group.
  [ChromeEarlGreyUI openTabGrid];
  OpenTabGroupCreationViewUsingLongPressForCellAtIndex(0);
  SetTabGroupCreationName(kGroup1Name);
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGroupCreationView()];

  // Create a second group.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(1)]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetPluralNSStringF(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTABTOTABGROUP,
                                   1))] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AddTabToGroupSubMenuButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGroupCreationView()];
  SetTabGroupCreationName(kGroup2Name);
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGroupCreationView()];

  // Open a second window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Search in the second window for the first group of the first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(@"1gr")];

  // Tap on it in the second window.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(kGroup1Name, 1)]
      performAction:grey_tap()];

  // Verify that it opens in the first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGroupView()];
  [[EarlGrey selectElementWithMatcher:TabGroupViewTitle(kGroup1Name)]
      assertWithMatcher:grey_notNil()];

  // Tap on it again in the second window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(kGroup1Name, 1)]
      performAction:grey_tap()];

  // Verify that it's still open in the first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGroupView()];
  [[EarlGrey selectElementWithMatcher:TabGroupViewTitle(kGroup1Name)]
      assertWithMatcher:grey_notNil()];

  // Search in the second window for the second group of the first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(@"2gr")];

  // Tap on it in the second window.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(kGroup2Name, 1)]
      performAction:grey_tap()];

  // Verify that it opens in the first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGroupView()];
  [[EarlGrey selectElementWithMatcher:TabGroupViewTitle(kGroup2Name)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the TabGrid is correctly updated when it was presenting a group
// before being backgrounded while incognito reauth is enabled.
- (void)testIncognitoReauth {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  OpenTabGroupAtIndex(0);

  [ChromeEarlGrey setBoolValue:YES
             forLocalStatePref:prefs::kIncognitoAuthenticationSetting];
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridNewIncognitoTabButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridEditButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_notVisible()];

  // Label of the button used to reauth.
  NSString* buttonLabel = l10n_util::GetNSStringF(
      IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON_VOICEOVER_LABEL,
      base::SysNSStringToUTF16(BiometricAuthenticationTypeString()));
  [[EarlGrey selectElementWithMatcher:testing::ButtonWithAccessibilityLabel(
                                          buttonLabel)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kIncognitoAuthenticationSetting];

  // Reset the app to make sure the incognito shield is removed.
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

// Tests that the group view is correctly updated after backgrounding and
// foregrounding the app.
- (void)testBackgroundingGroupViewWithMultipleNTPs {
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  // Open a second NTP.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openTabGrid];

  // Move it to the group.
  AddTabAtIndexToGroupWithTitle(
      1, l10n_util::GetPluralNSStringF(IDS_IOS_TAB_GROUP_TABS_NUMBER, 1));

  // Open a third NTP.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openTabGrid];

  // Move it to the group.
  AddTabAtIndexToGroupWithTitle(
      1, l10n_util::GetPluralNSStringF(IDS_IOS_TAB_GROUP_TABS_NUMBER, 2));

  OpenTabGroupAtIndex(0);

  // Check the UI before backgrounding the app.
  [self verifyVisibleTabsCount:3];
  [[EarlGrey
      selectElementWithMatcher:TabGroupViewTitle(l10n_util::GetPluralNSStringF(
                                   IDS_IOS_TAB_GROUP_TABS_NUMBER, 3))]
      assertWithMatcher:grey_notNil()];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  [self verifyVisibleTabsCount:1];
  [[EarlGrey
      selectElementWithMatcher:TabGroupViewTitle(l10n_util::GetPluralNSStringF(
                                   IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      assertWithMatcher:grey_notNil()];
}

// Tests opening a tab from the group view.
- (void)testOpenTabFromGroupView {
  std::string URL1 = "chrome://version";
  std::string URL2 = "chrome://about";
  std::string content1 = "Revision";
  std::string content2 = "List of Chrome URLs";

  // Load the first website.
  [ChromeEarlGrey loadURL:GURL(URL1)];
  [ChromeEarlGrey waitForWebStateContainingText:content1];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(URL1)]
      assertWithMatcher:grey_notNil()];
  NSString* versionTabTitle = [ChromeEarlGrey currentTabTitle];

  // Load the second website and this one should be the selected one.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:GURL(URL2)];
  [ChromeEarlGrey waitForWebStateContainingText:content2];
  NSString* aboutTabTitle = [ChromeEarlGrey currentTabTitle];

  // Create and open the group with the first loaded website.
  [ChromeEarlGreyUI openTabGrid];
  CreateDefaultFirstGroupFromTabCellAtIndex(0);
  OpenTabGroupAtIndex(0);
  [[EarlGrey selectElementWithMatcher:TabWithTitle(versionTabTitle)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(aboutTabTitle)]
      assertWithMatcher:grey_nil()];

  // Open the tab (currently not the selected one) and ensure this is the same
  // website loaded previously.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(versionTabTitle)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:content1];

  // Open it again as it is the selected one now.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(versionTabTitle)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:content1];

  // Go back to tab grid and check the group view is open.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(versionTabTitle)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(aboutTabTitle)]
      assertWithMatcher:grey_nil()];

  // Go back to the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGroupBackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:TabGridGroupCellAtIndex(0)];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(versionTabTitle)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(aboutTabTitle)]
      assertWithMatcher:grey_notNil()];
}

// Ensures inactive tabs are moved correctly when creating a group from search
// result.
- (void)testCreateGroupFromInactiveTab {
  // This test is not relevant on iPads because there is no inactive tabs in
  // iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad.");
  }

  std::string URL1 = "chrome://version";
  std::string content1 = "Revision";

  // Load the first website.
  [ChromeEarlGrey loadURL:GURL(URL1)];
  [ChromeEarlGrey waitForWebStateContainingText:content1];
  NSString* versionTabTitle = [ChromeEarlGrey currentTabTitle];

  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tab count should be 0");

  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      "--enable-features=" + std::string(kTabInactivityThreshold.name) + ":" +
      kTabInactivityThresholdParameterName + "/" +
      kTabInactivityThresholdImmediateDemoParam);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [ChromeEarlGreyUI openTabGrid];

  // The Inactive Tabs button should be visible.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kInactiveTabsButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // There should be one inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 1,
                 @"Inactive tab count should be 1");

  // The previously opened website should be in inactive now.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(versionTabTitle)]
      assertWithMatcher:grey_nil()];

  // Search for the inactive tab.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(versionTabTitle)];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(versionTabTitle)]
      assertWithMatcher:grey_notNil()];

  // Create a group with the displayed tab.
  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSearchCancelButton()]
      performAction:grey_tap()];
  // The Inactive Tabs button should be no longer present in the grid.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kInactiveTabsButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];

  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tab count should be 0");

  // The inactive tab should be in the group now.
  OpenTabGroupAtIndex(1);
  [[EarlGrey selectElementWithMatcher:TabWithTitle(versionTabTitle)]
      assertWithMatcher:grey_notNil()];
}

// Ensures to create a group from a pinned tab and the tab is no longer pinned.
- (void)testCreateGroupFromPinnedTab {
  // This test is not relevant on iPads because there is no pinned tabs in iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad.");
  }

  CreatePinnedTabs(1, self.testServer);
  [ChromeEarlGreyUI openTabGrid];

  // Create a group from the pinned tab.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab0")]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:
          ContextMenuItemWithAccessibilityLabel(l10n_util::GetPluralNSStringF(
              IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP, 1))]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGroupCreationView()];
  SetTabGroupCreationName(kGroup1Name);
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButton()]
      performAction:grey_tap()];

  // Ensure the pinned area and the pinned tab disappeared and the group cell
  // appeared.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForPinnedCellWithTitle(@"PinnedTab0")]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              @"PinnedViewIdentifier"),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

// Tests the group creation with a tab from another window using search result.
- (void)testCreateGroupFromTabInAnotherWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab2Title)];

  // Opens a new window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Open a link in the 2nd window.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)
       inWindowWithNumber:1];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];

  // Switch back to the first window and search for the tab in 2nd window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(kTab1Title)];

  // Create a group from search result.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:
          ContextMenuItemWithAccessibilityLabel(l10n_util::GetPluralNSStringF(
              IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP, 1))]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSearchCancelButton()]
      performAction:grey_tap()];

  // Check the tab been moved in a group in the first window and no longer
  // present in the second one.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab2Title)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_nil()];
  OpenTabGroupAtIndex(1);
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab2Title)]
      assertWithMatcher:grey_nil()];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab2Title)]
      assertWithMatcher:grey_nil()];

  [ChromeEarlGrey closeAllExtraWindows];
}

// Tests closing a group in grid using the selection mode.
- (void)testCloseFromSelection {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  // Tap on "Edit" then "Select tabs".
  [[EarlGrey selectElementWithMatcher:TabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];

  // Select the group.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 1),
                                          1)] performAction:grey_tap()];

  // Tap on the "Close Tab" button and confirm.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridEditCloseTabsButton()]
      performAction:grey_tap()];
  NSString* closeTabsButtonText =
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_TAB_GRID_CLOSE_ALL_TABS_CONFIRMATION,
          /*number=*/1));
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                   closeTabsButtonText)]
      performAction:grey_tap()];

  // Make sure that the tab grid is empty.
  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:0];

  // Check that the snackbar is displayed.
  [[EarlGrey selectElementWithMatcher:TabGroupSnackBar(1)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests closing a group in grid using the selection mode with kTabGroupSync
// disabled.
- (void)testCloseFromSelectionSyncDisabled {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  // Tap on "Edit" then "Select tabs".
  [[EarlGrey selectElementWithMatcher:TabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridSelectTabsMenuButton()]
      performAction:grey_tap()];

  // Select the group.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER, 1),
                                          1)] performAction:grey_tap()];

  // Tap on the "Close Tab" button and confirm.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridEditCloseTabsButton()]
      performAction:grey_tap()];
  NSString* closeTabsButtonText =
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_TAB_GRID_CLOSE_ALL_TABS_CONFIRMATION,
          /*number=*/1));
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                   closeTabsButtonText)]
      performAction:grey_tap()];

  // Make sure that the tab grid is empty.
  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:0];
}

@end
