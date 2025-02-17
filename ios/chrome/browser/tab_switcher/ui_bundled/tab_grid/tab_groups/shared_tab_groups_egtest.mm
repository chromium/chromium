// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/public/group_data.h"
#import "components/data_sharing/test_support/test_utils.h"
#import "components/sync/base/command_line_switches.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/share_kit/model/test_constants.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_app_interface.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_eg_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/test/query_title_server_util.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/test/tabs_egtest_util.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::CreateTabGroupAtIndex;
using chrome_test_util::DeleteGroupButton;
using chrome_test_util::DeleteGroupConfirmationButton;
using chrome_test_util::DeleteSharedConfirmationButton;
using chrome_test_util::DeleteSharedGroupButton;
using chrome_test_util::FakeJoinFlowView;
using chrome_test_util::FakeManageFlowView;
using chrome_test_util::FakeShareFlowView;
using chrome_test_util::KeepSharedConfirmationButton;
using chrome_test_util::LeaveSharedGroupButton;
using chrome_test_util::LeaveSharedGroupConfirmationButton;
using chrome_test_util::ManageGroupButton;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::NavigationBarSaveButton;
using chrome_test_util::OpenTabGroupAtIndex;
using chrome_test_util::ShareGroupButton;
using chrome_test_util::TabGridCellAtIndex;
using chrome_test_util::TabGridDoneButton;
using chrome_test_util::TabGridGroupCellAtIndex;
using chrome_test_util::TabGroupBackButton;
using chrome_test_util::TabGroupOverflowMenuButton;

namespace {

NSString* const kTab1Title = @"Tab1";
NSString* const kTab2Title = @"Tab2";
NSString* const kSharedTabTitle = @"Google";

// Put the number at the beginning to avoid issues with sentence case, as the
// keyboard default can differ iPhone vs iPad, simulator vs device.
NSString* const kGroup1Name = @"1group";

// Matcher for the face pile button.
id<GREYMatcher> FacePileButton() {
  return grey_accessibilityID(kTabGroupFacePileButtonIdentifier);
}

// Long presses a tab group cell.
void LongPressTabGroupCellAtIndex(unsigned int index) {
  // Make sure the cell has appeared. Otherwise, long pressing can be flaky.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:TabGridGroupCellAtIndex(index)];
  [ChromeEarlGreyUI waitForAppToIdle];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(index)]
      performAction:grey_longPress()];
}

// Shares the group at `index`.
void ShareGroupAtIndex(unsigned int index) {
  // Share the first group.
  LongPressTabGroupCellAtIndex(index);
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      performAction:grey_tap()];

  // Verify that this opened the fake Share flow.
  [[EarlGrey selectElementWithMatcher:FakeShareFlowView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Actually share the group.
  [[EarlGrey selectElementWithMatcher:NavigationBarSaveButton()]
      performAction:grey_tap()];

  // Wait for the disappearance of the Share Flow View and appearance of the
  // underlying Done button from the Tab Grid.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:FakeShareFlowView()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGridDoneButton()];
}

// Adds a shared tab group. User's role depends on its fake identity.
void AddSharedGroup() {
  [TabGroupAppInterface prepareFakeSharedTabGroups:1];
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];
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
  config.features_enabled.push_back(kTabGroupIndicator);
  config.features_enabled.push_back(shared_tab_group_flavor);

  // Add the flag to use FakeTabGroupSyncService.
  config.additional_args.push_back(
      "--" + std::string(test_switches::kEnableFakeTabGroupSyncService));

  return config;
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
  RegisterQueryTitleHandler(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");

  // Remove the user education screen by default.
  [ChromeEarlGrey
      setUserDefaultsObject:@YES
                     forKey:kSharedTabGroupUserEducationShownOnceKey];

  // `fakeIdentity2` joins shared groups as member.
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  if ([self isRunningTest:@selector(testLastTabClosedOwnerAlert)] ||
      [self
          isRunningTest:@selector(testShareGroupAndDeleteUsingContextMenus)] ||
      [self isRunningTest:@selector
            (testShareGroupAndDeleteFromGroupViewUsingContextMenus)]) {
    // `fakeIdentity2` joins shared groups as owner.
    identity = [FakeSystemIdentity fakeIdentity2];
  }
  [SigninEarlGreyUI signinWithFakeIdentity:identity enableHistorySync:YES];
}

- (void)tearDownHelper {
  [super tearDownHelper];
  // Delete all groups.
  [TabGroupAppInterface cleanup];
}

// Tests that the user education is shown in the grid only once.
- (void)testUserEducationInGrid {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }

  [ChromeEarlGrey
      removeUserDefaultsObjectForKey:kSharedTabGroupUserEducationShownOnceKey];

  [ChromeEarlGreyUI openTabGrid];
  CreateTabGroupAtIndex(0, kGroup1Name);

  ShareGroupAtIndex(0);

  // Open a tab group to trigger the user education screen.
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
  [[EarlGrey selectElementWithMatcher:TabGroupBackButton()]
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
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Share the first group.
  LongPressTabGroupCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      performAction:grey_tap()];

  // Verify that this opened the fake Share flow.
  [[EarlGrey selectElementWithMatcher:FakeShareFlowView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Cancel the Share flow.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Verify that it closes the Share flow.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:FakeShareFlowView()];

  // Verify that the group is not shared by checking that the context menu
  // offers to Share rather than Manage the group.
  LongPressTabGroupCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ManageGroupButton()]
      assertWithMatcher:grey_notVisible()];
}

// Checks opening the Share flow from the Tab Grid and actually sharing. Then
// checks opening the Manage flow. Using the face pile.
- (void)testShareGroupAndManageGroupUsingFacePile {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Open the tab group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Tap on the face pile to share the group.
  [[EarlGrey selectElementWithMatcher:FacePileButton()]
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

  // Tap on the face pile to manage the group.
  [[EarlGrey selectElementWithMatcher:FacePileButton()]
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

// Checks opening the Share flow from the Tab Grid and actually sharing. Then
// checks opening the Manage flow. Using context menus.
- (void)testShareGroupAndManageGroupUsingContextMenus {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Share the first group.
  LongPressTabGroupCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      performAction:grey_tap()];

  // Verify that this opened the fake Share flow.
  [[EarlGrey selectElementWithMatcher:FakeShareFlowView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Actually share the group.
  [[EarlGrey selectElementWithMatcher:NavigationBarSaveButton()]
      performAction:grey_tap()];

  // Verify that it closes the Share flow.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:FakeShareFlowView()];

  // Verify that the group is shared by checking that the context menu offers to
  // Manage rather than Share the group.
  LongPressTabGroupCellAtIndex(0);
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
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  [TabGroupAppInterface mockSharedEntitiesPreview];
  GURL joinGroupURL = data_sharing::GetDataSharingUrl(data_sharing::GroupToken(
      data_sharing::GroupId("resources%2F3be"), "CggHBicxA_slvx"));
  [ChromeEarlGrey loadURL:joinGroupURL waitForCompletion:NO];

  // Verify that it opened the Join flow.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:FakeJoinFlowView()];

  // Close the Join flow.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Verify that it closed the Join flow.
  [[EarlGrey selectElementWithMatcher:FakeJoinFlowView()]
      assertWithMatcher:grey_notVisible()];
}

// Checks that the IPH is presented when the user foreground the app with a
// shared tab group active.
- (void)testForegroundIPH {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  if ([ChromeEarlGrey isIPadIdiom]) {
    // Not available on iPad.
    return;
  }

  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.iph_feature_enabled = "IPH_iOSSharedTabGroupForeground";
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                         enableHistorySync:YES];

  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab2Title)];
  [ChromeEarlGreyUI openTabGrid];
  CreateTabGroupAtIndex(1, kGroup1Name);
  [[EarlGrey selectElementWithMatcher:TabGridDoneButton()]
      performAction:grey_tap()];

  // Foreground the app. The group is not shared so no IPH.
  AppLaunchManager* manager = [AppLaunchManager sharedManager];
  [manager backgroundAndForegroundApp];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_SHARED_GROUP_USER_EDUCATION_IPH_FOREGROUND))]
      assertWithMatcher:grey_nil()];

  // Share the group then foreground the app. The IPH should be visible.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGroupBackButton()]
      performAction:grey_tap()];
  ShareGroupAtIndex(1);
  [[EarlGrey selectElementWithMatcher:TabGridDoneButton()]
      performAction:grey_tap()];
  [manager backgroundAndForegroundApp];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_SHARED_GROUP_USER_EDUCATION_IPH_FOREGROUND))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks opening the Share flow from the Tab Grid and actually sharing. Then
// deleting the shared group as owner.
- (void)testShareGroupAndDeleteUsingContextMenus {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  AddSharedGroup();

  // Long press the group.
  LongPressTabGroupCellAtIndex(0);

  // Verify that the leave button is not available.
  [[EarlGrey selectElementWithMatcher:LeaveSharedGroupButton()]
      assertWithMatcher:grey_notVisible()];

  // Delete the shared group.
  [[EarlGrey selectElementWithMatcher:DeleteSharedGroupButton()]
      performAction:grey_tap()];
  // Tap on the delete button again to confirm the deletion.
  [[EarlGrey selectElementWithMatcher:DeleteSharedConfirmationButton()]
      performAction:grey_tap()];

  // Check that the group is deleted.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGridGroupCellAtIndex(0)];
}

// Checks joining a group. Then leaving the shared group as member.
- (void)testJoinGroupAndLeaveUsingContextMenus {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  AddSharedGroup();

  // Long press the group.
  LongPressTabGroupCellAtIndex(0);

  // Verify that the delete button is not available.
  [[EarlGrey selectElementWithMatcher:DeleteGroupButton()]
      assertWithMatcher:grey_notVisible()];

  // Leave the shared group.
  [[EarlGrey selectElementWithMatcher:LeaveSharedGroupButton()]
      performAction:grey_tap()];
  // Tap on the leave button confirmation.
  [[EarlGrey selectElementWithMatcher:LeaveSharedGroupConfirmationButton()]
      performAction:grey_tap()];

  // Check that the group is removed locally.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGridGroupCellAtIndex(0)];
}

// Checks opening the Share flow from the Tab Grid and actually sharing. Then
// deleting the shared group from the group view as owner.
- (void)testShareGroupAndDeleteFromGroupViewUsingContextMenus {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  AddSharedGroup();

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Display the tab group overflow menu.
  [[EarlGrey selectElementWithMatcher:TabGroupOverflowMenuButton()]
      performAction:grey_tap()];

  // Verify that the leave button is not available.
  [[EarlGrey selectElementWithMatcher:LeaveSharedGroupButton()]
      assertWithMatcher:grey_notVisible()];

  // Delete the shared group.
  [[EarlGrey selectElementWithMatcher:DeleteSharedGroupButton()]
      performAction:grey_tap()];
  // Tap on the delete button again to confirm the deletion.
  [[EarlGrey selectElementWithMatcher:DeleteSharedConfirmationButton()]
      performAction:grey_tap()];

  // Check that the group view is closed and the group deleted.
  [[EarlGrey selectElementWithMatcher:TabGroupOverflowMenuButton()]
      assertWithMatcher:grey_notVisible()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGridGroupCellAtIndex(0)];
}

// Checks joining a group. Then leaving the shared group from the group view as
// member.
- (void)testJoinGroupAndLeaveFromGroupViewUsingContextMenus {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  AddSharedGroup();

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Display the tab group overflow menu.
  [[EarlGrey selectElementWithMatcher:TabGroupOverflowMenuButton()]
      performAction:grey_tap()];

  // Verify that the delete button is not available.
  [[EarlGrey selectElementWithMatcher:DeleteGroupButton()]
      assertWithMatcher:grey_notVisible()];

  // Leave the shared group.
  [[EarlGrey selectElementWithMatcher:LeaveSharedGroupButton()]
      performAction:grey_tap()];
  // Tap on the leave button confirmation.
  [[EarlGrey selectElementWithMatcher:LeaveSharedGroupConfirmationButton()]
      performAction:grey_tap()];

  // Check that the group view is closed and the group leaved.
  [[EarlGrey selectElementWithMatcher:TabGroupOverflowMenuButton()]
      assertWithMatcher:grey_notVisible()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGridGroupCellAtIndex(0)];
}

// Tests that TabGroupAppInterface creates shared tab groups correctly.
- (void)testPreparedSharedGroupsAtStartup {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  [TabGroupAppInterface prepareFakeSharedTabGroups:3];

  [ChromeEarlGreyUI openTabGrid];

  GREYAssertEqual(3, [TabGroupAppInterface countOfSavedTabGroups],
                  @"The number of saved tab groups should be 3.");

  // Verify that 3 shared tab groups created by `+prepareFakeSharedTabGroups:`
  // exist in the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(1)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(2)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(3)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the context menu offers to Manage button instead of the Share
  // button.
  LongPressTabGroupCellAtIndex(1);
  [[EarlGrey selectElementWithMatcher:ManageGroupButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      assertWithMatcher:grey_notVisible()];
}

// Checks last tab close alert as owner of the group open a new tab and close
// the last tab, when "Keep Group" is pressed and delete the group when "Delete
// Group" is pressed.
- (void)testLastTabClosedOwnerAlert {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  AddSharedGroup();

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Check that kSharedTabTitle tab cell is in the group.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kSharedTabTitle)]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Close the tab and check the alert propose to delete and not to leave the
  // group as the user is an owner.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:DeleteSharedConfirmationButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:LeaveSharedGroupConfirmationButton()]
      assertWithMatcher:grey_notVisible()];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Tap on "Keep Group"
  [[EarlGrey selectElementWithMatcher:KeepSharedConfirmationButton()]
      performAction:grey_tap()];

  // Verify that the tab group view is still displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kTabGroupViewIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Check that kSharedTabTitle tab cell is in not in the group anymore.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kSharedTabTitle)]
      assertWithMatcher:grey_nil()];

  // Wait until the page has finished loading.
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Ensure the new tab is a new tab page.
  const GURL currentURL = [ChromeEarlGrey webStateVisibleURL];
  const GURL expectedURL(kChromeUINewTabURL);
  GREYAssertEqual(expectedURL, currentURL, @"Page navigated unexpectedly to %s",
                  currentURL.spec().c_str());

  [ChromeEarlGrey waitForMainTabCount:1];
  // Close the tab and this time, delete the group.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:DeleteSharedConfirmationButton()]
      performAction:grey_tap()];

  // Check that the group view is closed and the group deleted.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGridGroupCellAtIndex(0)];
  [ChromeEarlGrey waitForMainTabCount:0];
}

// Ensures the last tab close alert as a member is displayed when the group is
// shared.
- (void)testLastTabClosedMemberAlert {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  AddSharedGroup();

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Ensure the tab is not a new tab page.
  const GURL newTabPageURL(kChromeUINewTabURL);
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  const GURL tabInGroupURL = [ChromeEarlGrey webStateVisibleURL];
  GREYAssertNotEqual(newTabPageURL, tabInGroupURL,
                     @"The tab should not be a new tab page.");
  [ChromeEarlGreyUI openTabGrid];

  // Close the tab and check the alert propose to leave and not delete the group
  // as the user is a member.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:LeaveSharedGroupConfirmationButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:KeepSharedConfirmationButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DeleteSharedConfirmationButton()]
      assertWithMatcher:grey_notVisible()];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Tap on "Keep Group"
  [[EarlGrey selectElementWithMatcher:KeepSharedConfirmationButton()]
      performAction:grey_tap()];

  // Verify that the tab group view is still displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kTabGroupViewIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Wait until the page has finished loading.
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Ensure the new tab is a new tab page.
  const GURL currentURL = [ChromeEarlGrey webStateVisibleURL];
  GREYAssertEqual(newTabPageURL, currentURL,
                  @"Page navigated unexpectedly to %s",
                  currentURL.spec().c_str());

  [ChromeEarlGrey waitForMainTabCount:1];
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

- (void)setUp {
  [super setUp];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                         enableHistorySync:YES];
}

// Checks that the user with JoinOnly rights can't start the Share flow from
// Tab Grid.
- (void)testCantShareGroup {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Try to share the first group.
  LongPressTabGroupCellAtIndex(0);

  // Verify that there is no Share or Manage button.
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:ManageGroupButton()]
      assertWithMatcher:grey_nil()];
}

// Checks that the user with JoinOnly can trigger the Join flow.
- (void)testJoinGroup {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  [TabGroupAppInterface mockSharedEntitiesPreview];
  GURL joinGroupURL = data_sharing::GetDataSharingUrl(data_sharing::GroupToken(
      data_sharing::GroupId("resources%2F3be"), "CggHBicxA_slvx"));
  [ChromeEarlGrey loadURL:joinGroupURL waitForCompletion:NO];

  // Verify that it opened the Join flow.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:FakeJoinFlowView()];

  // Close the Join flow.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Verify that it closed the Join flow.
  [[EarlGrey selectElementWithMatcher:FakeJoinFlowView()]
      assertWithMatcher:grey_notVisible()];
}

@end
