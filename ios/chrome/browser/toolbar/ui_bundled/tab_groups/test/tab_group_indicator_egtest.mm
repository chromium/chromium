// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/public/group_data.h"
#import "components/data_sharing/test_support/test_utils.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/share_kit/model/test_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_app_interface.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/test/query_title_server_util.h"
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/ui/tab_group_indicator_constants.h"
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

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ContextMenuItemWithAccessibilityLabel;
using chrome_test_util::CreateTabGroupCreateButton;
using chrome_test_util::DeleteGroupButton;
using chrome_test_util::DeleteSharedConfirmationButton;
using chrome_test_util::DeleteSharedGroupButton;
using chrome_test_util::FakeJoinFlowView;
using chrome_test_util::FakeShareFlowView;
using chrome_test_util::LeaveSharedGroupButton;
using chrome_test_util::LeaveSharedGroupConfirmationButton;
using chrome_test_util::NavigationBarSaveButton;
using chrome_test_util::ShareGroupButton;
using chrome_test_util::TabGridCellAtIndex;
using chrome_test_util::TabGridCloseButtonForCellAtIndex;
using chrome_test_util::TabGridGroupCellAtIndex;
using chrome_test_util::TabGroupCreationView;

namespace {

NSString* const kTab1Title = @"Tab1";
NSString* const kTab2Title = @"Tab2";

NSString* const kGroupTitle = @"Group Title";

// Matcher for the tab group indicator view.
id<GREYMatcher> TabGroupIndicatorViewMatcher() {
  return grey_allOf(
      grey_ancestor(grey_accessibilityID(kTabGroupIndicatorViewIdentifier)),
      grey_sufficientlyVisible(), nil);
}

// Matcher for the Tab Grid button in its kNormal style with the given tab
// count text.
id<GREYMatcher> TabGridButtonInNormalStyle(NSString* tabCountText) {
  return grey_allOf(ButtonWithAccessibilityLabelId(IDS_IOS_TOOLBAR_SHOW_TABS),
                    grey_accessibilityValue(tabCountText), nil);
}

// Matcher for the Tab Grid button in its kTabGroup style with the given tab
// count text.
id<GREYMatcher> TabGridButtonInTabGroupStyle(NSString* tabCountText) {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_TOOLBAR_SHOW_TAB_GROUP),
      grey_accessibilityValue(tabCountText), nil);
}

// Returns a matcher for the tab group indicator view with `title` as title.
id<GREYMatcher> TabGroupIndicatorViewMatcherWithGroupTitle(NSString* title) {
  return grey_allOf(
      grey_ancestor(grey_accessibilityID(kTabGroupIndicatorViewIdentifier)),
      grey_accessibilityLabel(title), grey_sufficientlyVisible(), nil);
}

// Returns a matcher for a menu button with `accessibility_label` as
// accessibility label.
id<GREYMatcher> MenuButtonMatcher(int accessibility_label_id) {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(accessibility_label_id),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

// Displays the tab cell context menu by long pressing at the tab cell at
// `tab_cell_index`.
void DisplayContextMenuForTabCellAtIndex(int tab_cell_index) {
  // TODO(crbug.com/443957909): Investigate if there is a better solution to fix
  // flakiness on iOS26.
  base::PlatformThread::Sleep(base::Seconds(1));
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(tab_cell_index)]
      performAction:grey_longPress()];
}

// Creates a group with default title from a tab cell at index `tab_cell_index`
// when no group is in the grid.
void CreateDefaultFirstGroupFromTabCellAtIndex(int tab_cell_index) {
  DisplayContextMenuForTabCellAtIndex(tab_cell_index);

  // TODO(crbug.com/443957909): Investigate if there is a better solution to fix
  // flakiness on iOS26.
  base::PlatformThread::Sleep(base::Seconds(1));
  [[EarlGrey
      selectElementWithMatcher:
          ContextMenuItemWithAccessibilityLabel(l10n_util::GetPluralNSStringF(
              IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP, 1))]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGroupCreationView()];
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButton()]
      performAction:grey_tap()];
}

// Creates a default tab group and open the tab group indicator menu.
void CreateDefaultTabGroupAndOpenMenu(
    net::test_server::EmbeddedTestServer* testServer) {
  // Create a tab cell.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  // Create a group.
  CreateDefaultFirstGroupFromTabCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Open the tab group indicator menu.
  [[EarlGrey
      selectElementWithMatcher:TabGroupIndicatorViewMatcherWithGroupTitle(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      performAction:grey_tap()];
}

// Creates a shared tab group with a test URL and opens the tab group
// indicator menu.
void CreateSharedGroupAndOpenMenu(
    net::test_server::EmbeddedTestServer* test_server) {
  NSString* url =
      base::SysUTF8ToNSString(GetQueryTitleURL(test_server, kTab1Title).spec());
  [TabGroupAppInterface prepareFakeSharedTabGroups:1 asOwner:NO url:url];
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGridCellAtIndex(0)];
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupIndicatorViewMatcherWithGroupTitle(
                                   @"shared group")] performAction:grey_tap()];
}

}  // namespace

// Tests for the tab group indicator.
@interface TabGroupIndicatorTestCase : ChromeTestCase
@end

@implementation TabGroupIndicatorTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      data_sharing::features::kDataSharingFeature);
  config.features_disabled.push_back(kIOSAutoOpenRemoteTabGroupsSettings);

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

  // `fakeIdentity2` joins shared groups as member.
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  if ([self isRunningTest:@selector
            (testTabGroupIndicatorMenuActionsDeleteSharedGroup)]) {
    // `fakeIdentity2` joins shared groups as owner.
    identity = [FakeSystemIdentity fakeIdentity2];
  }
  [SigninEarlGreyUI signinWithFakeIdentity:identity enableHistorySync:YES];
}

- (void)tearDownHelper {
  [super tearDownHelper];
  // Delete groups.
  [TabGroupAppInterface cleanup];
}

// Tests that the tab group indicator view is visible when the active tab is
// grouped.
- (void)testTabGroupIndicatorVisibility {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"On iPad, the tab group indicator is not displayed "
                           @"if the tab strip is visible.");
  }

  // Check that the indicator is not visible.
  [[EarlGrey selectElementWithMatcher:TabGroupIndicatorViewMatcher()]
      assertWithMatcher:grey_nil()];

  // Create a tab cell.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  // Create a group.
  CreateDefaultFirstGroupFromTabCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Check that the indicator is visible.
  [[EarlGrey
      selectElementWithMatcher:TabGroupIndicatorViewMatcherWithGroupTitle(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the indicator is not visible in landscape.
  [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationLandscapeLeft
                                   error:nil];
  [[EarlGrey selectElementWithMatcher:TabGroupIndicatorViewMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Check that the indicator is visible when switching back to portrait.
  [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationPortrait
                                   error:nil];
  [[EarlGrey
      selectElementWithMatcher:TabGroupIndicatorViewMatcherWithGroupTitle(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 1))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the tab group indicator view is visible when the active tab is
// grouped.
- (void)testTabGroupIndicatorNotVisibleOnIpad {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped on iPhone.");
  }

  // Check that the indicator is not visible.
  [[EarlGrey selectElementWithMatcher:TabGroupIndicatorViewMatcher()]
      assertWithMatcher:grey_nil()];

  // Create a tab cell.
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openTabGrid];

  // Create a group.
  CreateDefaultFirstGroupFromTabCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Check that the indicator is still not visible.
  [[EarlGrey selectElementWithMatcher:TabGroupIndicatorViewMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that opening a new tab in the group from the tab group indicator menu
// works.
- (void)testTabGroupIndicatorMenuActionsOpenNewTab {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"On iPad, the tab group indicator is not displayed "
                           @"if the tab strip is visible.");
  }
  CreateDefaultTabGroupAndOpenMenu(self.testServer);

  // Tap on the "New tab in group" button.
  [[EarlGrey
      selectElementWithMatcher:MenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_NEWTABINGROUP)]
      performAction:grey_tap()];

  // Check that there are now two tabs and the current tab has changed.
  GREYAssertEqual(2UL, [ChromeEarlGrey mainTabCount],
                  @"Expected 2 tabs to be present.");
  NSString* newTabTitle = [ChromeEarlGrey currentTabTitle];
  GREYAssertNotEqual(kTab1Title, newTabTitle,
                     @"New current tab should have a different title");
}

// Tests that menu actions are correct.
- (void)testTabGroupIndicatorMenuActions {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"On iPad, the tab group indicator is not displayed "
                           @"if the tab strip is visible.");
  }
  CreateDefaultTabGroupAndOpenMenu(self.testServer);

  // Check that displayed menu actions are correct.
  [[EarlGrey
      selectElementWithMatcher:MenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_NEWTABINGROUP)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:MenuButtonMatcher(
                                          IDS_IOS_CONTENT_CONTEXT_UNGROUP)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:MenuButtonMatcher(
                                          IDS_IOS_CONTENT_CONTEXT_RENAMEGROUP)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:MenuButtonMatcher(
                                          IDS_IOS_CONTENT_CONTEXT_DELETEGROUP)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:MenuButtonMatcher(
                                          IDS_IOS_CONTENT_CONTEXT_CLOSEGROUP)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that closing a tab group from the tab group indicator menu works.
- (void)testTabGroupIndicatorMenuActionsCloseGroup {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"On iPad, the tab group indicator is not displayed "
                           @"if the tab strip is visible.");
  }
  CreateDefaultTabGroupAndOpenMenu(self.testServer);

  // Tap on the "Close Group" button.
  [[EarlGrey selectElementWithMatcher:MenuButtonMatcher(
                                          IDS_IOS_CONTENT_CONTEXT_CLOSEGROUP)]
      performAction:grey_tap()];

  // Check that there are now 0 tab.
  GREYAssertEqual(0UL, [ChromeEarlGrey mainTabCount],
                  @"Expected 0 tab to be present.");

  // Tap on the snackbar action.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGroupSnackBarAction()]
      performAction:grey_tap()];

  // Check that the tab groups page is shown.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGroupsPanel()]
      assertWithMatcher:grey_notNil()];
}

// Tests that deleting a tab group from the tab group indicator menu works.
- (void)testTabGroupIndicatorMenuActionsDeleteGroup {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"On iPad, the tab group indicator is not displayed "
                           @"if the tab strip is visible.");
  }
  CreateDefaultTabGroupAndOpenMenu(self.testServer);

  // Tap on the "Delete Group" button.
  [[EarlGrey selectElementWithMatcher:MenuButtonMatcher(
                                          IDS_IOS_CONTENT_CONTEXT_DELETEGROUP)]
      performAction:grey_tap()];

  // Confirm deleting a group.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          DeleteGroupConfirmationButton()]
      performAction:grey_tap()];

  // Check that there are now 0 tab.
  GREYAssertEqual(0UL, [ChromeEarlGrey mainTabCount],
                  @"Expected 0 tab to be present.");
}

// Tests that renaming a tab group from the tab group indicator menu works.
- (void)testTabGroupIndicatorMenuActionsRenameGroup {
  // TODO(crbug.com/442817314): Re-enable this flaky test on iOS26.
  if (base::ios::IsRunningOnIOS26OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.");
  }

  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"On iPad, the tab group indicator is not displayed "
                           @"if the tab strip is visible.");
  }
  CreateDefaultTabGroupAndOpenMenu(self.testServer);

  // Tap on the "Rename Group" button.
  [[EarlGrey selectElementWithMatcher:MenuButtonMatcher(
                                          IDS_IOS_CONTENT_CONTEXT_RENAMEGROUP)]
      performAction:grey_tap()];

  // Rename the tab group to `kGroupTitle`.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              kCreateTabGroupViewIdentifier)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kCreateTabGroupTextFieldIdentifier)]
      performAction:grey_replaceText(kGroupTitle)];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(kCreateTabGroupViewIdentifier)];

  // Check that the group has been renamed.
  [[EarlGrey
      selectElementWithMatcher:TabGroupIndicatorViewMatcherWithGroupTitle(
                                   kGroupTitle)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that ungrouping a tab group from the tab group indicator menu works.
- (void)testTabGroupIndicatorMenuActionsUngroup {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"On iPad, the tab group indicator is not displayed "
                           @"if the tab strip is visible.");
  }
  CreateDefaultTabGroupAndOpenMenu(self.testServer);

  // Tap on the "Ungroup" button.
  [[EarlGrey selectElementWithMatcher:MenuButtonMatcher(
                                          IDS_IOS_CONTENT_CONTEXT_UNGROUP)]
      performAction:grey_tap()];

  // Confirm ungrouping.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::UngroupConfirmationButton()]
      performAction:grey_tap()];

  // Check that the indicator is not visible.
  [[EarlGrey selectElementWithMatcher:TabGroupIndicatorViewMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that the tab group indicator is correctly displayed on the NTP.
- (void)testTabGroupIndicatorNTP {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"On iPad, the tab indicator is not displayed if "
                           @"the tab strip is visible.");
  }

  // Check that the indicator is not visible.
  [[EarlGrey selectElementWithMatcher:TabGroupIndicatorViewMatcher()]
      assertWithMatcher:grey_nil()];

  CreateDefaultTabGroupAndOpenMenu(self.testServer);

  // Tap on the "New tab in group" button.
  [[EarlGrey
      selectElementWithMatcher:MenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_NEWTABINGROUP)]
      performAction:grey_tap()];

  // Check that there are now two tabs and the current tab has changed.
  GREYAssertEqual(2UL, [ChromeEarlGrey mainTabCount],
                  @"Expected 2 tabs to be present.");
  NSString* newTabTitle = [ChromeEarlGrey currentTabTitle];
  GREYAssertNotEqual(kTab1Title, newTabTitle,
                     @"New current tab should have a different title");

  // Check that the indicator is visible.
  [[EarlGrey
      selectElementWithMatcher:TabGroupIndicatorViewMatcherWithGroupTitle(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 2))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Scroll down the NTP and check that the indicator is not visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:TabGroupIndicatorViewMatcher()]
      assertWithMatcher:grey_nil()];

  // Scroll up the NTP and check that the indicator is visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];
  [[EarlGrey
      selectElementWithMatcher:TabGroupIndicatorViewMatcherWithGroupTitle(
                                   l10n_util::GetPluralNSStringF(
                                       IDS_IOS_TAB_GROUP_TABS_NUMBER, 2))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the Tab Grid button indicator is correctly updated whether a tab
// is grouped or not.
- (void)testTabGridButtonUpdatesWhenTabIsGroupedUngrouped {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"On iPad, the tab group indicator is not displayed "
                           @"if the tab strip is visible.");
  }
  // Open a second tab.
  [ChromeEarlGreyUI openNewTab];

  // Check that the Tab Grid button is in normal style, with 2 tabs.
  [[EarlGrey selectElementWithMatcher:TabGridButtonInNormalStyle(@"2")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Create a group.
  [ChromeEarlGreyUI openTabGrid];
  CreateDefaultFirstGroupFromTabCellAtIndex(1);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Check that the Tab Grid button is in kTabGroup style, with 1 tab.
  [[EarlGrey selectElementWithMatcher:TabGridButtonInTabGroupStyle(@"1")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Ungroup the tab.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:MenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_MOVETABTOGROUP)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:MenuButtonMatcher(
                                   IDS_IOS_CONTENT_CONTEXT_REMOVEFROMGROUP)]
      performAction:grey_tap()];

  // Open the tab.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Check that the Tab Grid button is in normal style, with 2 tabs.
  [[EarlGrey selectElementWithMatcher:TabGridButtonInNormalStyle(@"2")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that deleting a shared tab group from the tab group indicator menu
// works.
- (void)testTabGroupIndicatorMenuActionsDeleteSharedGroup {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"On iPad, the tab group indicator is not displayed "
                           @"if the tab strip is visible.");
  }
  CreateSharedGroupAndOpenMenu(self.testServer);

  // Verify that the leave button is not available.
  [[EarlGrey selectElementWithMatcher:LeaveSharedGroupButton()]
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

// Tests that leaving a shared tab group from the tab group indicator menu
// works.
- (void)testTabGroupIndicatorMenuActionsLeaveSharedGroup {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"On iPad, the tab group indicator is not displayed "
                           @"if the tab strip is visible.");
  }
  CreateSharedGroupAndOpenMenu(self.testServer);

  // Verify that the delete button is not available.
  [[EarlGrey selectElementWithMatcher:DeleteGroupButton()]
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
                                   return [ChromeEarlGrey mainTabCount] == 0;
                                 }];
  bool groupsLeaved = [groupsLeavedCheck waitWithTimeout:10];
  GREYAssertTrue(groupsLeaved, @"Failed to leave the shared group");
}

@end
