// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_constants.h"
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

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ContextMenuItemWithAccessibilityLabel;
using chrome_test_util::ContextMenuItemWithAccessibilityLabelId;
using chrome_test_util::TabGridCellAtIndex;
using chrome_test_util::TabGridEditAddToButton;
using chrome_test_util::TabGridEditButton;
using chrome_test_util::TabGridEditMenuCloseAllButton;
using chrome_test_util::TabGridGroupCellAtIndex;
using chrome_test_util::TabGridNewTabButton;
using chrome_test_util::TabGridSearchBar;
using chrome_test_util::TabGridSearchTabsButton;
using chrome_test_util::TabGridSelectTabsMenuButton;
using chrome_test_util::TabGridUndoCloseAllButton;
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

// Returns the matcher for the tab group creation view.
id<GREYMatcher> GroupCreationViewMatcher() {
  return grey_allOf(grey_accessibilityID(kCreateTabGroupViewIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Returns the matcher for `Create Group` button.
id<GREYMatcher> CreateGroupButtonInGroupCreation() {
  return grey_allOf(grey_accessibilityID(kCreateTabGroupCreateButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Returns the matcher for the tab group view.
id<GREYMatcher> GroupViewMatcher() {
  return grey_allOf(grey_accessibilityID(kTabGroupViewIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Returns the matcher for the title on the Tab Group view.
id<GREYMatcher> GroupViewTitle(NSString* title) {
  return grey_allOf(grey_accessibilityID(kTabGroupViewTitleIdentifier),
                    grey_accessibilityLabel(title), nil);
}

// Returns the matcher for the sub menu button `Add Tab to New Group`.
id<GREYMatcher> NewTabGroupButton() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP_SUBMENU);
}

// Returns the matcher for `Rename Group` button.
id<GREYMatcher> RenameGroupButton() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_RENAMEGROUP);
}

// Returns the matcher for `Ungroup` button.
id<GREYMatcher> UngroupButton() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_UNGROUP);
}

// Returns the matcher for `Delete Group` button.
id<GREYMatcher> DeleteGroupButton() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_DELETEGROUP);
}

// Identifier for cell at given `index` in the tab grid.
NSString* IdentifierForTabCellAtIndex(unsigned int index) {
  return [NSString stringWithFormat:@"%@%u", kGridCellIdentifierPrefix, index];
}

// Matcher for the tab cell at the given `index`.
id<GREYMatcher> TabCellMatcherAtIndex(unsigned int index) {
  return grey_allOf(grey_accessibilityID(IdentifierForTabCellAtIndex(index)),
                    grey_kindOfClassName(@"GridCell"),
                    grey_sufficientlyVisible(), nil);
}

// Returns the matcher for the overflow menu button.
id<GREYMatcher> TabGroupOverflowMenuButtonMatcher() {
  return grey_allOf(grey_accessibilityID(kTabGroupOverflowMenuButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
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
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:GroupCreationViewMatcher()];
  [[EarlGrey selectElementWithMatcher:CreateGroupButtonInGroupCreation()]
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
  [[EarlGrey selectElementWithMatcher:NewTabGroupButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:GroupCreationViewMatcher()];
  [[EarlGrey selectElementWithMatcher:CreateGroupButtonInGroupCreation()]
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
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabCellMatcherAtIndex(0)];
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
id<GREYMatcher> TabGroupGridCellMatcher(NSString* group_name) {
  return grey_allOf(grey_accessibilityLabel(group_name),
                    grey_kindOfClassName(@"GroupGridCell"),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> AddTabToNewGroupButton() {
  return grey_allOf(
      ContextMenuItemWithAccessibilityLabel(l10n_util::GetPluralNSStringF(
          IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP, 1)),
      grey_sufficientlyVisible(), nil);
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

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:GroupCreationViewMatcher()];
}

// Sets the tab group name in the tab group creation view.
void SetTabGroupCreationName(NSString* group_name) {
  [[EarlGrey selectElementWithMatcher:CreateTabGroupTextFieldMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:group_name flags:0];
}

// Renames the group cell at index `group_cell_index` with `title`.
void RenameGroupAtIndex(int group_cell_index, NSString* title) {
  DisplayContextMenuForGroupCellAtIndex(group_cell_index);
  [[EarlGrey selectElementWithMatcher:RenameGroupButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:GroupCreationViewMatcher()];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:title flags:0];
  [[EarlGrey selectElementWithMatcher:CreateGroupButtonInGroupCreation()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:GroupCreationViewMatcher()];
}

// Ungroups the group cell at index `group_cell_index`.
void UngroupGroupAtIndex(int group_cell_index) {
  DisplayContextMenuForGroupCellAtIndex(group_cell_index);
  [[EarlGrey selectElementWithMatcher:UngroupButton()]
      performAction:grey_tap()];
}

// Deletes the group cell at index `group_cell_index`.
void DeleteGroupAtIndex(int group_cell_index) {
  DisplayContextMenuForGroupCellAtIndex(group_cell_index);
  [[EarlGrey selectElementWithMatcher:DeleteGroupButton()]
      performAction:grey_tap()];
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
  config.features_enabled.push_back(kTabGroupsInGrid);
  config.features_enabled.push_back(kTabGroupsIPad);
  config.features_enabled.push_back(kModernTabStrip);
  config.features_enabled.push_back(kTabGroupSync);
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
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButtonMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:GroupCreationViewMatcher()];

  // Open the group.
  [[EarlGrey selectElementWithMatcher:TabGroupGridCellMatcher(kGroup1Name)]
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
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCancelButtonMatcher()]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:GroupCreationViewMatcher()];
  [[EarlGrey selectElementWithMatcher:TabGroupGridCellMatcher(kGroup1Name)]
      assertWithMatcher:grey_nil()];
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
  [[EarlGrey selectElementWithMatcher:TabGroupGridCellMatcher(kGroup1Name)]
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
  [[EarlGrey
      selectElementWithMatcher:TabGroupGridCellMatcher(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      assertWithMatcher:grey_nil()];
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
  [[EarlGrey
      selectElementWithMatcher:TabGroupGridCellMatcher(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      assertWithMatcher:grey_nil()];
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
  [[EarlGrey
      selectElementWithMatcher:TabGroupGridCellMatcher(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      assertWithMatcher:grey_nil()];

  // The IPH is shown.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_TAB_GRID_SAVED_TAB_GROUPS)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the second close doesn't trigger the IPH.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];
  CreateDefaultFirstGroupFromTabCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::TabGridCloseButtonForGroupCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupGridCellMatcher(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      assertWithMatcher:grey_nil()];

  // The IPH is *not* shown.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_TAB_GRID_SAVED_TAB_GROUPS)]
      assertWithMatcher:grey_nil()];
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

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:GroupCreationViewMatcher()];
  [[EarlGrey selectElementWithMatcher:CreateGroupButtonInGroupCreation()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:GroupCreationViewMatcher()];

  // `Tab 1` tab cell no longer present in the grid.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_nil()];

  // The group with title `1 Tab` is present in the grid.
  [[EarlGrey
      selectElementWithMatcher:TabGroupGridCellMatcher(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      assertWithMatcher:grey_notNil()];
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
  [[EarlGrey
      selectElementWithMatcher:TabGroupGridCellMatcher(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      assertWithMatcher:grey_nil()];

  // The group with title `2 Tabs` is present in the grid.
  [[EarlGrey
      selectElementWithMatcher:TabGroupGridCellMatcher(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 2))]
      assertWithMatcher:grey_notNil()];
}

// Checks that all the options are displayed in the group's overflow menu.
- (void)testAppropriateOverflowMenuInGroupView {
  // Create a tab cell with `Tab 1` as its title.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  CreateDefaultFirstGroupFromTabCellAtIndex(0);

  OpenTabGroupAtIndex(0);

  // Display the tab group overflow menu.
  [[EarlGrey selectElementWithMatcher:TabGroupOverflowMenuButtonMatcher()]
      performAction:grey_tap()];

  // Check the different buttons.
  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                          IDS_IOS_CONTENT_CONTEXT_RENAMEGROUP)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                          IDS_IOS_CONTENT_CONTEXT_UNGROUP)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                          IDS_IOS_CONTENT_CONTEXT_DELETEGROUP)]
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
  [[EarlGrey
      selectElementWithMatcher:TabGroupGridCellMatcher(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      assertWithMatcher:grey_notNil()];

  // Close all (groups and tabs).
  [[EarlGrey selectElementWithMatcher:TabGridEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridEditMenuCloseAllButton()]
      performAction:grey_tap()];

  // Check that `Tab 2` and the group with title `1 Tab` are no longer in the
  // grid.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab2Title)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupGridCellMatcher(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      assertWithMatcher:grey_nil()];

  // Tap Undo button.
  [[EarlGrey selectElementWithMatcher:TabGridUndoCloseAllButton()]
      performAction:grey_tap()];

  // Check that `Tab 2` and the group with title `1 Tab` are back in the grid.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab2Title)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupGridCellMatcher(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      assertWithMatcher:grey_notNil()];
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
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButtonMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:GroupCreationViewMatcher()];

  // Create a second group.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(1)]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetPluralNSStringF(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTABTOTABGROUP,
                                   1))] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:NewTabGroupButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:GroupCreationViewMatcher()];
  SetTabGroupCreationName(kGroup2Name);
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButtonMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:GroupCreationViewMatcher()];

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
  [[EarlGrey selectElementWithMatcher:TabGroupGridCellMatcher(kGroup1Name)]
      performAction:grey_tap()];

  // Verify that it opens in the first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:GroupViewMatcher()];
  [[EarlGrey selectElementWithMatcher:GroupViewTitle(kGroup1Name)]
      assertWithMatcher:grey_notNil()];

  // Tap on it again in the second window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:TabGroupGridCellMatcher(kGroup1Name)]
      performAction:grey_tap()];

  // Verify that it's still open in the first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:GroupViewMatcher()];
  [[EarlGrey selectElementWithMatcher:GroupViewTitle(kGroup1Name)]
      assertWithMatcher:grey_notNil()];

  // Search in the second window for the second group of the first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(@"2gr")];

  // Tap on it in the second window.
  [[EarlGrey selectElementWithMatcher:TabGroupGridCellMatcher(kGroup2Name)]
      performAction:grey_tap()];

  // Verify that it opens in the first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:GroupViewMatcher()];
  [[EarlGrey selectElementWithMatcher:GroupViewTitle(kGroup2Name)]
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
      selectElementWithMatcher:GroupViewTitle(l10n_util::GetPluralNSStringF(
                                   IDS_IOS_TAB_GROUP_TABS_NUMBER, 3))]
      assertWithMatcher:grey_notNil()];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  [self verifyVisibleTabsCount:1];
  [[EarlGrey
      selectElementWithMatcher:GroupViewTitle(l10n_util::GetPluralNSStringF(
                                   IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      assertWithMatcher:grey_notNil()];
}

// Tests the different explanation text (signed in/out) in the creation screen.
- (void)testTabGroupCreationScreenExplanation {
  [ChromeEarlGreyUI openTabGrid];

  // Open the creation view.
  OpenTabGroupCreationViewUsingLongPressForCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_TAB_GROUP_CREATION_SAVED_EXPLANATION)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Cancel the creation.
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCancelButtonMatcher()]
      performAction:grey_tap()];

  // Sign in.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [SigninEarlGrey setSelectedType:syncer::UserSelectableType::kTabs
                          enabled:YES];

  // Open the creation view.
  OpenTabGroupCreationViewUsingLongPressForCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_TAB_GROUP_CREATION_SYNC_EXPLANATION)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Cancel the creation.
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCancelButtonMatcher()]
      performAction:grey_tap()];
}

@end
