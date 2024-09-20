// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift_constants_for_objective_c.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::CreateTabGroupCancelButton;
using chrome_test_util::CreateTabGroupCreateButton;
using chrome_test_util::CreateTabGroupTextField;
using chrome_test_util::DeleteGroupConfirmationButton;
using chrome_test_util::TabGroupSnackBar;
using chrome_test_util::TabGroupSnackBarAction;
using chrome_test_util::UngroupConfirmationButton;

namespace {

NSString* const kGroupTitle1 = @"Group Title 1";
NSString* const kGroupTitle2 = @"Group Title 2";

NSString* const kTab1Title = @"Tab1";
NSString* const kTab2Title = @"Tab2";

// Put the number at the beginning to avoid issues with sentence case.
NSString* const kGroupName = @"1group";

// Returns the matcher for the tab group creation view.
id<GREYMatcher> HalfVisibleTabGroupCreationView() {
  return grey_allOf(grey_accessibilityID(kCreateTabGroupViewIdentifier),
                    grey_minimumVisiblePercent(0.5), nil);
}

// Returns the matcher for the Tab Groups view as third panel of Tab Grid.
id<GREYMatcher> TabGroupsPanel() {
  return grey_allOf(grey_accessibilityID(kTabGroupsPanelIdentifier),
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
      waitForUIElementToAppearWithMatcher:HalfVisibleTabGroupCreationView()];
}

// Sets the tab group name in the tab group creation view.
void SetTabGroupCreationName(NSString* group_name) {
  [[EarlGrey selectElementWithMatcher:CreateTabGroupTextField()]
      performAction:grey_tap()];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:group_name flags:0];
}

// Identifer for cell at given `index` in the tab grid.
NSString* IdentifierForRegularCellAtIndex(unsigned int index) {
  return [NSString stringWithFormat:@"%@%u",
                                    TabStripCollectionViewConstants
                                        .tabStripTabCellPrefixIdentifier,
                                    index];
}

// Returns a matcher for a tab strip tab cell with `title` as title.
id<GREYMatcher> TabStripTabCellMatcher(NSString* title) {
  return grey_allOf(grey_kindOfClassName(@"UIView"),
                    grey_not(grey_kindOfClassName(@"UILabel")),
                    grey_accessibilityLabel(title),
                    grey_ancestor(grey_kindOfClassName(@"TabStripTabCell")),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the currently selected tab strip tab cell.
id<GREYMatcher> TabStripTabCellSelectedMatcher() {
  return grey_allOf(grey_kindOfClassName(@"UIView"),
                    grey_not(grey_kindOfClassName(@"UILabel")),
                    grey_accessibilityTrait(UIAccessibilityTraitSelected),
                    grey_ancestor(grey_kindOfClassName(@"TabStripTabCell")),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for a tab strip group cell.
id<GREYMatcher> TabStripGroupCellMatcher() {
  return grey_allOf(grey_kindOfClassName(@"TabStripGroupCell"),
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

// Matcher for an unnamed tab group that has the `number_of_tabs`.
id<GREYMatcher> CreateUnnamedTabGroupMatcher(int number_of_tabs) {
  return TabStripGroupCellMatcher(l10n_util::GetPluralNSStringF(
      IDS_IOS_TAB_GROUP_TABS_NUMBER, number_of_tabs));
}

// Returns a matcher for a context menu button with `accessibility_label` as
// accessibility label.
id<GREYMatcher> ContextMenuButtonMatcher(NSString* accessibility_label) {
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabel(accessibility_label),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
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
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kCreateTabGroupTextFieldIdentifier)]
      performAction:grey_replaceText(group_title)];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(kCreateTabGroupViewIdentifier)];
}

// Finds the element with the given `identifier` of given `type`.
XCUIElement* GetElementMatchingIdentifier(XCUIApplication* app,
                                          NSString* identifier,
                                          XCUIElementType type) {
  XCUIElementQuery* query = [[app.windows.firstMatch
      descendantsMatchingType:type] matchingIdentifier:identifier];
  return [query elementBoundByIndex:0];
}

// Drags and drops the cell with the given `src_cell_identifier` to the
// `dst_cell_identifier` position.
void DragDropTabStripTabCellInTabStripView(NSString* src_cell_identifier,
                                           NSString* dst_cell_identifier) {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* src_element = GetElementMatchingIdentifier(
      app, src_cell_identifier, XCUIElementTypeCell);
  XCUICoordinate* start_point =
      [src_element coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.5)];

  XCUIElement* dst_element = GetElementMatchingIdentifier(
      app, dst_cell_identifier, XCUIElementTypeCell);
  XCUICoordinate* end_point =
      [dst_element coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.5)];

  [start_point pressForDuration:1.5
           thenDragToCoordinate:end_point
                   withVelocity:XCUIGestureVelocityDefault
            thenHoldForDuration:1.0];
}

}  // namespace

// Tests for the tab strip shown on iPad.
@interface TabStripGroupTestCase : ChromeTestCase
@end

@implementation TabStripGroupTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabGroupsIPad);
  config.features_enabled.push_back(kModernTabStrip);
  config.features_enabled.push_back(kTabGroupSync);
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
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:HalfVisibleTabGroupCreationView()];
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupName)]
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
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCancelButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:HalfVisibleTabGroupCreationView()];
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupName)]
      assertWithMatcher:grey_nil()];
}

// Tests that a tab can be closed using its context menu.
- (void)testTabStripContextMenuCloseTab {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  // Long press the current tab.
  [[EarlGrey selectElementWithMatcher:TabStripTabCellSelectedMatcher()]
      performAction:grey_longPress()];

  // Tab the "Close Tab" button in the context menu.
  [[EarlGrey selectElementWithMatcher:ContextMenuButtonMatcher(
                                          IDS_IOS_CONTENT_CONTEXT_CLOSETAB)]
      performAction:grey_tap()];

  // Wait for the current tab cell to disappear.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabStripTabCellSelectedMatcher()];
}

// Tests that a tab can be closed using the close button in the tab cell.
- (void)testTabStripCloseTab {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:TabStripTabCellSelectedMatcher()];

  // Tap the close button of the current tab cell.
  id<GREYMatcher> currentTabCloseButtonMatcher = grey_allOf(
      grey_accessibilityID(
          TabStripTabItemConstants.closeButtonAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:currentTabCloseButtonMatcher]
      performAction:grey_tap()];

  // Wait for the current tab cell to disappear.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabStripTabCellSelectedMatcher()];
}

// Tests that a tab can be added to a new named group using the context menu.
- (void)testTabStripCreateNewNamedGroupWithTab {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  // Add the current tab to a new group with title `kGroupTitle1`.
  AddTabToNewGroup(TabStripTabCellSelectedMatcher(), kGroupTitle1);
  // Test that there is now a group cell with title `kGroupTitle1`.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];
}

// Tests that adding a tab to an unnamed group increases the count in the title.
- (void)testTabStripAddingToUnnamedGroupIncreasesCountInTitle {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  id<GREYMatcher> groupWithOneTabMatcher = CreateUnnamedTabGroupMatcher(1);
  id<GREYMatcher> groupWithTwoTabsMatcher = CreateUnnamedTabGroupMatcher(2);
  id<GREYMatcher> contextMenuButtonMatcher =
      ContextMenuButtonMatcher(IDS_IOS_CONTENT_CONTEXT_NEWTABINGROUP);

  // Add the current tab to a new group with an empty title.
  AddTabToNewGroup(TabStripTabCellSelectedMatcher(), @"");

  // When created, the unnamed tab group should use the tabs count as title.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:groupWithOneTabMatcher];

  // Long press the tab group and tab "New Tab in Group".
  [[EarlGrey selectElementWithMatcher:groupWithOneTabMatcher]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:contextMenuButtonMatcher]
      performAction:grey_tap()];

  // Check that there are now two tabs and the current tab has changed.
  GREYAssertEqual(2, [ChromeEarlGrey mainTabCount],
                  @"Expected 2 tabs to be present.");

  // Then the count that appears in the title should have been increased.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:groupWithTwoTabsMatcher];
}

// Tests that a tab can be added to a new unnamed group using the context menu.
- (void)testTabStripCancelTabGroupCreation {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  id<GREYMatcher> tab_cell_matcher = TabStripTabCellSelectedMatcher();
  [[EarlGrey selectElementWithMatcher:tab_cell_matcher]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:ContextMenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP,
                                   1)] performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              kCreateTabGroupViewIdentifier)];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(
                                       kCreateTabGroupCancelButtonIdentifier),
                                   grey_interactable(), nil)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(kCreateTabGroupViewIdentifier)];

  // Test that there is no tab group.
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that a tab can be removed from a tab group.
- (void)testTabStripRemoveTabFromGroup {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  // Add the current tab to a group with title `kGroupTitle1`.
  AddTabToNewGroup(TabStripTabCellSelectedMatcher(), kGroupTitle1);

  // Wait for the group cell to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];

  // Long press the current tab, open the "Move Tab to Group" sub-menu and then
  // tap "Remove from Group".
  [[EarlGrey selectElementWithMatcher:TabStripTabCellSelectedMatcher()]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:ContextMenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_MOVETABTOGROUP)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ContextMenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_REMOVEFROMGROUP)]
      performAction:grey_tap()];

  // Long press the current tab and check the context menu now contains "Add Tab
  // to New Group" instead of "Move Tab to Group".
  [[EarlGrey selectElementWithMatcher:TabStripTabCellSelectedMatcher()]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:ContextMenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP,
                                   1)] assertWithMatcher:grey_notNil()];
}

// Tests that a tab can be added to an existing tab group.
- (void)testTabStripAddTabToGroup {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  // Load "chrome://about" and "chrome://version" in two different tabs.
  [ChromeEarlGrey loadURL:GURL("chrome://about")];
  NSString* aboutTabTitle = [ChromeEarlGrey currentTabTitle];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // Add the tab "chrome://version" to a group with title `kGroupTitle1`.
  AddTabToNewGroup(TabStripTabCellSelectedMatcher(), kGroupTitle1);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];

  // Long press the tab "chrome://about" and add it to the existing group using
  // the context menu.
  [[EarlGrey selectElementWithMatcher:TabStripTabCellMatcher(aboutTabTitle)]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:ContextMenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTABTOTABGROUP, 1)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ContextMenuButtonMatcher(kGroupTitle1)]
      performAction:grey_tap()];

  // Long press the tab "chrome://about" and check the context menu now contains
  // "Move Tab to Group" instead of "Add Tab to Group".
  [[EarlGrey selectElementWithMatcher:TabStripTabCellMatcher(aboutTabTitle)]
      performAction:grey_longPress()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          ContextMenuButtonMatcher(IDS_IOS_CONTENT_CONTEXT_CLOSETAB)];
  [[EarlGrey
      selectElementWithMatcher:ContextMenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_MOVETABTOGROUP)]
      assertWithMatcher:grey_notNil()];
}

// Tests that a tab can be moved to another tab group.
- (void)testTabStripMoveTabToGroup {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  [ChromeEarlGrey loadURL:GURL("chrome://about")];
  NSString* aboutTabTitle = [ChromeEarlGrey currentTabTitle];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  NSString* versionTabTitle = [ChromeEarlGrey currentTabTitle];

  // Add the tab "chrome://about" to a group with title `kGroupTitle1`.
  AddTabToNewGroup(TabStripTabCellMatcher(aboutTabTitle), kGroupTitle1);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];

  // Add the tab "chrome://version" to a group with title `kGroupTitle2`.
  AddTabToNewGroup(TabStripTabCellMatcher(versionTabTitle), kGroupTitle2,
                   /*sub_menu=*/true);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle2)];

  // Long press the tab "chrome://about" and move it to the other group using
  // the context menu.
  [[EarlGrey selectElementWithMatcher:TabStripTabCellMatcher(aboutTabTitle)]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:ContextMenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_MOVETABTOGROUP)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ContextMenuButtonMatcher(kGroupTitle2)]
      performAction:grey_tap()];

  // Long press the tab "chrome://about" and check the context menu still
  // contains "Move Tab to Group".
  [[EarlGrey selectElementWithMatcher:TabStripTabCellMatcher(aboutTabTitle)]
      performAction:grey_longPress()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          ContextMenuButtonMatcher(IDS_IOS_CONTENT_CONTEXT_CLOSETAB)];
  [[EarlGrey
      selectElementWithMatcher:ContextMenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_MOVETABTOGROUP)]
      assertWithMatcher:grey_notNil()];
  // Check that the tab group with title `kGroupTitle1` disappeared.
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle1)]
      assertWithMatcher:grey_nil()];
}

// Tests that a tab group can be collapsed and expanded.
- (void)testTabStripCollapseExpandGroup {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  [ChromeEarlGrey loadURL:GURL("chrome://about")];
  NSString* aboutTabTitle = [ChromeEarlGrey currentTabTitle];
  // Add the tab "chrome://about" to a new tab group.
  AddTabToNewGroup(TabStripTabCellSelectedMatcher(), kGroupTitle1);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];
  [[EarlGrey selectElementWithMatcher:TabStripTabCellMatcher(aboutTabTitle)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Collapse the tab group and check that the tab disappeared.
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle1)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:TabStripTabCellMatcher(
                                                             aboutTabTitle)];
  // Expand the tab group and check that the tab reappeared.
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle1)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripTabCellMatcher(
                                                          aboutTabTitle)];
}

// Tests that a tab group can be renamed.
- (void)testTabStripRenameGroup {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  // Add the current tab to a new tab group with title `kGroupTitle1`.
  AddTabToNewGroup(TabStripTabCellSelectedMatcher(), kGroupTitle1);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];

  // Long press the tab group and tap "Rename Group".
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle1)]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:ContextMenuButtonMatcher(
                                          IDS_IOS_CONTENT_CONTEXT_RENAMEGROUP)]
      performAction:grey_tap()];

  // Rename the tab group to `kGroupTitle2`.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              kCreateTabGroupViewIdentifier)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kCreateTabGroupTextFieldIdentifier)]
      performAction:grey_replaceText(kGroupTitle2)];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(kCreateTabGroupViewIdentifier)];

  // Check that the title was changed to `kGroupTitle2`.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle2)];
}

// Tests that a new tab can be added to a tab group.
- (void)testTabStripAddNewTabInGroup {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  [ChromeEarlGrey loadURL:GURL("chrome://about")];
  NSString* aboutTabTitle = [ChromeEarlGrey currentTabTitle];

  // Add the tab "chrome://about" to a new tab group.
  AddTabToNewGroup(TabStripTabCellSelectedMatcher(), kGroupTitle1);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];

  // Long press the tab group and tab "New Tab in Group".
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle1)]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:ContextMenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_NEWTABINGROUP)]
      performAction:grey_tap()];

  // Check that there are now two tabs and the current tab has changed.
  GREYAssertEqual(2, [ChromeEarlGrey mainTabCount],
                  @"Wrong number of opened tabs");
  NSString* newTabTitle = [ChromeEarlGrey currentTabTitle];
  GREYAssertNotEqual(aboutTabTitle, newTabTitle,
                     @"New current tab should have a different title");

  // Long press the current tab and check that there is "Move Tab to Group" in
  // the context menu.
  [[EarlGrey selectElementWithMatcher:TabStripTabCellSelectedMatcher()]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:ContextMenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_MOVETABTOGROUP)]
      assertWithMatcher:grey_notNil()];
}

// Tests that a tab group can be ungrouped.
- (void)testTabStripUngroupGroup {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  [ChromeEarlGrey loadURL:GURL("chrome://about")];
  NSString* aboutTabTitle = [ChromeEarlGrey currentTabTitle];

  // Add the current tab to a new group.
  AddTabToNewGroup(TabStripTabCellSelectedMatcher(), kGroupTitle1);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];

  // Long press the tab group and tap "Ungroup".
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle1)]
      performAction:grey_longPress()];
  // Tap a ungroup button.
  [[EarlGrey selectElementWithMatcher:ContextMenuButtonMatcher(
                                          IDS_IOS_CONTENT_CONTEXT_UNGROUP)]
      performAction:grey_tap()];
  // Confirm ungrouping.
  [[EarlGrey selectElementWithMatcher:UngroupConfirmationButton()]
      performAction:grey_tap()];

  // Wait for the tab group to disappear and check that the tab is still here.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabStripGroupCellMatcher(
                                                 kGroupTitle1)];
  [[EarlGrey selectElementWithMatcher:TabStripTabCellMatcher(aboutTabTitle)]
      assertWithMatcher:grey_notNil()];
}

// Tests that a tab group can be deleted.
- (void)testTabStripDeleteGroup {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  // Open a new tab and load "chrome://about".
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://about")];
  NSString* aboutTabTitle = [ChromeEarlGrey currentTabTitle];

  // Add the current tab to a new group.
  AddTabToNewGroup(TabStripTabCellSelectedMatcher(), kGroupTitle1);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];

  // Long press the tab group and tap "Delete Group".
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle1)]
      performAction:grey_longPress()];
  // Tap a delete button.
  [[EarlGrey selectElementWithMatcher:ContextMenuButtonMatcher(
                                          IDS_IOS_CONTENT_CONTEXT_DELETEGROUP)]
      performAction:grey_tap()];
  // Confirm deleting a group.
  [[EarlGrey selectElementWithMatcher:DeleteGroupConfirmationButton()]
      performAction:grey_tap()];

  // Wait for the tab group to disappear and check that the tab disappeared too.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabStripGroupCellMatcher(
                                                 kGroupTitle1)];
  [[EarlGrey selectElementWithMatcher:TabStripTabCellMatcher(aboutTabTitle)]
      assertWithMatcher:grey_nil()];

  // Check that the snackbar is not displayed.
  [[EarlGrey selectElementWithMatcher:TabGroupSnackBar(1)]
      assertWithMatcher:grey_nil()];
}

// Tests that a tab group can be closed.
- (void)testTabStripCloseGroup {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  // Open a new tab and load "chrome://about".
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://about")];
  NSString* aboutTabTitle = [ChromeEarlGrey currentTabTitle];

  // Add the current tab to a new group.
  AddTabToNewGroup(TabStripTabCellSelectedMatcher(), kGroupTitle1);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];

  // Long press the tab group and tap "Close Group".
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle1)]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:ContextMenuButtonMatcher(
                                          IDS_IOS_CONTENT_CONTEXT_CLOSEGROUP)]
      performAction:grey_tap()];

  // Wait for the tab group to disappear and check that the tab disappeared too.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabStripGroupCellMatcher(
                                                 kGroupTitle1)];
  [[EarlGrey selectElementWithMatcher:TabStripTabCellMatcher(aboutTabTitle)]
      assertWithMatcher:grey_nil()];

  // Check that the snackbar is displayed.
  [[EarlGrey selectElementWithMatcher:TabGroupSnackBar(1)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the "Close Other Tabs" action when a tab group is involved.
- (void)testTabStripCloseOtherTabsWithGroup {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  // Open tabs.
  [ChromeEarlGrey loadURL:GURL("chrome://about")];
  NSString* aboutTabTitle = [ChromeEarlGrey currentTabTitle];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  NSString* versionTabTitle = [ChromeEarlGrey currentTabTitle];

  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2,
                  @"Three tabs were expected to be open");

  // Add the first tab to a new group.
  AddTabToNewGroup(TabStripTabCellMatcher(aboutTabTitle), kGroupTitle1);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];

  // Long press the non grouped tab and tap "Close Other Tabs".
  [[EarlGrey selectElementWithMatcher:TabStripTabCellMatcher(versionTabTitle)]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:ContextMenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_CLOSEOTHERTABS)]
      performAction:grey_tap()];

  // Wait for other tabs to disappear.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabStripGroupCellMatcher(
                                                 kGroupTitle1)];
  [[EarlGrey selectElementWithMatcher:TabStripTabCellMatcher(versionTabTitle)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the snackbar is displayed.
  [[EarlGrey selectElementWithMatcher:TabGroupSnackBar(1)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the "Close Other Tabs" action when a tab group is not involved.
- (void)testTabStripCloseOtherTabsWithoutGroup {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  // Open tabs.
  [ChromeEarlGrey loadURL:GURL("chrome://about")];
  NSString* aboutTabTitle = [ChromeEarlGrey currentTabTitle];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  NSString* versionTabTitle = [ChromeEarlGrey currentTabTitle];

  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2,
                  @"Three tabs were expected to be open");

  // Add the first tab to a new group.
  AddTabToNewGroup(TabStripTabCellMatcher(aboutTabTitle), kGroupTitle1);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];

  // Long press the grouped tab and tap "Close Other Tabs".
  [[EarlGrey selectElementWithMatcher:TabStripTabCellMatcher(aboutTabTitle)]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:ContextMenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_CLOSEOTHERTABS)]
      performAction:grey_tap()];

  // Wait for other tabs to disappear.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:TabStripTabCellMatcher(
                                                             versionTabTitle)];
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle1)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the snackbar is not displayed.
  [[EarlGrey selectElementWithMatcher:TabGroupSnackBar(1)]
      assertWithMatcher:grey_nil()];
}

// Tests dragging the last tab out of a group then accepting to delete the
// group.
- (void)testDragLastTabOutOfGroupDelete {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://about")];

  AddTabToNewGroup(TabStripTabCellMatcher(@"About Version"), kGroupTitle1);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];

  // Tab0: New Tab
  // Group 1
  // Tab2 in group 1: About Version
  // Tab3, currently active: Chrome URLs

  // Move Tab2 to Tab3.
  DragDropTabStripTabCellInTabStripView(IdentifierForRegularCellAtIndex(2),
                                        IdentifierForRegularCellAtIndex(3));

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_DELETEGROUP)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle1)]
      assertWithMatcher:grey_nil()];
}

// Tests dragging the last tab out of a group then cancelling it.
- (void)testDragLastTabOutOfGroupCancel {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://about")];

  AddTabToNewGroup(TabStripTabCellMatcher(@"About Version"), kGroupTitle1);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];

  // Tab0: New Tab
  // Group 1
  // Tab2 in group 1: About Version
  // Tab3, currently active: Chrome URLs

  // Move Tab2 to Tab3.
  DragDropTabStripTabCellInTabStripView(IdentifierForRegularCellAtIndex(2),
                                        IdentifierForRegularCellAtIndex(3));

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_CANCEL)] performAction:grey_tap()];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];
}

// Tests that a tab group can be deleted.
- (void)testTabStripCancelConfirmation {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  // Open a new tab and load "chrome://about".
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://about")];
  NSString* aboutTabTitle = [ChromeEarlGrey currentTabTitle];

  // Add the current tab to a new group.
  AddTabToNewGroup(TabStripTabCellSelectedMatcher(), kGroupTitle1);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];

  // Long press the tab group and tap "Delete Group".
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle1)]
      performAction:grey_longPress()];
  // Tap a delete button.
  [[EarlGrey selectElementWithMatcher:ContextMenuButtonMatcher(
                                          IDS_IOS_CONTENT_CONTEXT_DELETEGROUP)]
      performAction:grey_tap()];
  // Cancel the action by tapping a tab title (= outside the delete button).
  [[EarlGrey selectElementWithMatcher:TabStripTabCellMatcher(aboutTabTitle)]
      performAction:grey_tap()];

  // Check that the group and the tab still exist.
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TabStripTabCellMatcher(aboutTabTitle)]
      assertWithMatcher:grey_notNil()];
}

// Tests the tab group snackbar CTA.
- (void)testTabStripTabGroupSnackbarAction {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  // Open a new tab and load "chrome://about".
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://about")];
  NSString* aboutTabTitle = [ChromeEarlGrey currentTabTitle];

  // Add the current tab to a new group.
  AddTabToNewGroup(TabStripTabCellSelectedMatcher(), kGroupTitle1);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle1)];

  // Long press the tab group and tap "Close Group".
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle1)]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:ContextMenuButtonMatcher(
                                          IDS_IOS_CONTENT_CONTEXT_CLOSEGROUP)]
      performAction:grey_tap()];

  // Wait for the tab group to disappear and check that the tab disappeared too.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabStripGroupCellMatcher(
                                                 kGroupTitle1)];
  [[EarlGrey selectElementWithMatcher:TabStripTabCellMatcher(aboutTabTitle)]
      assertWithMatcher:grey_nil()];

  // Tap on the snackbar action.
  [[EarlGrey selectElementWithMatcher:TabGroupSnackBarAction()]
      performAction:grey_tap()];

  // Check that the Tab Groups Panel is shown.
  [[EarlGrey selectElementWithMatcher:TabGroupsPanel()]
      assertWithMatcher:grey_notNil()];
}

@end
