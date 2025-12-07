// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/collaboration/public/features.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/public/group_data.h"
#import "components/data_sharing/test_support/test_utils.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/command_line_switches.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/share_kit/model/test_constants.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_app_interface.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_eg_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/test/query_title_server_util.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/test/tabs_egtest_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using chrome_test_util::AddTabToGroupSubMenuButton;
using chrome_test_util::BlueDotOnShowTabsButton;
using chrome_test_util::BlueDotOnTabStripCellAtIndex;
using chrome_test_util::CloseTabGroupButton;
using chrome_test_util::ContextMenuItemWithAccessibilityLabel;
using chrome_test_util::ContextMenuItemWithAccessibilityLabelId;
using chrome_test_util::CreateTabGroupAtIndex;
using chrome_test_util::CreateTabGroupCreateButton;
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
using chrome_test_util::LongPressTabGroupCellAtIndex;
using chrome_test_util::ManageGroupButton;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::NavigationBarSaveButton;
using chrome_test_util::NotificationDotOnTabStripGroupCellAtIndex;
using chrome_test_util::OpenTabGroupAtIndex;
using chrome_test_util::RecentActivityButton;
using chrome_test_util::ShareGroupButton;
using chrome_test_util::TabGridCellAtIndex;
using chrome_test_util::TabGridDoneButton;
using chrome_test_util::TabGridGroupCellAtIndex;
using chrome_test_util::TabGridNewTabButton;
using chrome_test_util::TabGroupActivityLabelOnGridCellAtIndex;
using chrome_test_util::TabGroupActivityLabelOnGroupCellAtIndex;
using chrome_test_util::TabGroupActivitySummaryCell;
using chrome_test_util::TabGroupActivitySummaryCellCloseButton;
using chrome_test_util::TabGroupCreationView;
using chrome_test_util::TabGroupOverflowMenuButton;
using chrome_test_util::TabGroupRecentActivityCellAtIndex;
using chrome_test_util::TabGroupViewTitle;
using chrome_test_util::TabStripCellAtIndex;
using chrome_test_util::TabStripGroupCellAtIndex;
using chrome_test_util::WindowWithNumber;
using data_sharing::features::kDataSharingFeature;
using data_sharing::features::kDataSharingJoinOnly;

namespace {

NSString* const kTab1Title = @"Tab1";
NSString* const kTab2Title = @"Tab2";
NSString* const kSharedTabTitle = @"Google";
NSString* const kSharedGroupTitle = @"shared group";

// Put the number at the beginning to avoid issues with sentence case, as the
// keyboard default can differ iPhone vs iPad, simulator vs device.
NSString* const kGroup1Name = @"1group";
NSString* const kGroup2Name = @"2group";

// Constant for timeout while waiting for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(10);

// Matcher for the face pile button.
id<GREYMatcher> FacePileButton() {
  return grey_accessibilityID(kTabGroupFacePileButtonIdentifier);
}

// Long press on the given matcher.
void LongPressOn(id<GREYMatcher> matcher) {
  // Ensure the element is visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:matcher];
  [ChromeEarlGreyUI waitForAppToIdle];
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_longPress()
                                                         error:&error];
    return error == nil;
  };

  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Long press failed.");
}

// Shares the group at `index`.
void ShareGroupAtIndex(unsigned int index) {
  // Share the first group.
  LongPressTabGroupCellAtIndex(index);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:ShareGroupButton()];
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

// Adds a shared tab group with a test URL and sets the user as `owner` or not
// of the group.
void AddSharedGroup(BOOL owner,
                    net::test_server::EmbeddedTestServer* test_server) {
  NSString* url = base::SysUTF8ToNSString(
      GetQueryTitleURL(test_server, kSharedTabTitle).spec());
  [TabGroupAppInterface prepareFakeSharedTabGroups:1 asOwner:owner url:url];
  [ChromeEarlGreyUI openTabGrid];
  // Close the tab grid once the button is available.
  id<GREYMatcher> closeButtonMatcher =
      chrome_test_util::TabGridCloseButtonForCellAtIndex(0);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:closeButtonMatcher];
  [[EarlGrey selectElementWithMatcher:closeButtonMatcher]
      performAction:grey_tap()];
}

// Returns the completely configured AppLaunchConfiguration (i.e. setting all
// the underlying feature dependencies), with the Shared Tab Groups flavor as a
// parameter.
AppLaunchConfiguration SharedTabGroupAppLaunchConfiguration(
    bool join_only = false) {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      collaboration::features::kCollaborationMessaging);
  if (join_only) {
    config.features_enabled.push_back(kDataSharingJoinOnly);
    config.features_disabled.push_back(kDataSharingFeature);
  } else {
    config.features_enabled.push_back(kDataSharingFeature);
  }
  config.features_disabled.push_back(kIOSAutoOpenRemoteTabGroupsSettings);

  // Add the flag to use FakeTabGroupSyncService.
  config.additional_args.push_back(
      "--" + std::string(test_switches::kEnableFakeTabGroupSyncService));

  return config;
}

// Waits for the fake join flow view to appear.
void WaitForFakeJoinFlowView() {
  // Verify that it opened the Join flow. Since it makes external calls and the
  // timeout is at 5 seconds, set a longer timeout here to ensure that the join
  // screen appears.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:FakeJoinFlowView()
                                              timeout:base::Seconds(20)];
}

}  // namespace

// Test Shared Tab Groups feature (with group creation access).
@interface SharedTabGroupsTestCase : ChromeTestCase
@end

@implementation SharedTabGroupsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  return SharedTabGroupAppLaunchConfiguration();
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
  [SigninEarlGreyUI signinWithFakeIdentity:identity enableHistorySync:YES];

  // Make sure that the MessagingBackendService is fully initialized.
  NSError* error = [ChromeEarlGrey waitForMessagingBackendServiceInitialized];
  GREYAssertNil(error, @"Failed to initialize MessagingBackendService: %@",
                error);
}

- (void)tearDownHelper {
  [super tearDownHelper];
  // Delete all groups.
  [TabGroupAppInterface cleanup];
}

- (void)testOpenURL {
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab2Title)];

  [ChromeEarlGrey selectTabAtIndex:0];

  GREYAssertEqual(0ul, [ChromeEarlGrey indexOfActiveNormalTab], @"Active");

  GURL joinGroupURL = data_sharing::GetDataSharingUrl(data_sharing::GroupToken(
      data_sharing::GroupId("resources%2F3be"), "CggHBicxA_slvx"));
  [ChromeEarlGrey sceneOpenURL:joinGroupURL];

  GREYAssertEqual(0ul, [ChromeEarlGrey indexOfActiveNormalTab], @"Active");
}

// Tests that the user education is shown in the grid only once.
- (void)testUserEducationInGrid {
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
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackPrimaryButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:educationScreen];
  [[EarlGrey selectElementWithMatcher:CloseTabGroupButton()]
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
  // TODO(crbug.com/441923004): Re-enable this test on iOS26.
  if (base::ios::IsRunningOnIOS26OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.");
  }

  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Open the tab group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Open the menu and check elements while not shared.
  [[EarlGrey selectElementWithMatcher:TabGroupOverflowMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close the menu by tapping outside the menu area.
  [[EarlGrey selectElementWithMatcher:CloseTabGroupButton()]
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
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:FakeShareFlowView()];

  // Open the menu and check elements while shared.
  [[EarlGrey selectElementWithMatcher:TabGroupOverflowMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ManageGroupButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:RecentActivityButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close the menu by tapping outside the menu area.
  [[EarlGrey selectElementWithMatcher:CloseTabGroupButton()]
      performAction:grey_tap()];

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
  [TabGroupAppInterface mockSharedEntitiesPreview];
  GURL joinGroupURL = data_sharing::GetDataSharingUrl(data_sharing::GroupToken(
      data_sharing::GroupId("resources%2F3be"), "CggHBicxA_slvx"));
  [ChromeEarlGrey loadURL:joinGroupURL waitForCompletion:NO];

  WaitForFakeJoinFlowView();

  // Close the Join flow.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Verify that it closed the Join flow.
  [[EarlGrey selectElementWithMatcher:FakeJoinFlowView()]
      assertWithMatcher:grey_notVisible()];
}

// Checks that the IPH is presented when the user foreground the app with a
// shared tab group active.
// TODO(crbug.com/411064928): Re-enable this test.
- (void)DISABLED_testForegroundIPH {
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
  [[EarlGrey selectElementWithMatcher:CloseTabGroupButton()]
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
  AddSharedGroup(/*owner=*/YES, self.testServer);

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
  AddSharedGroup(/*owner=*/NO, self.testServer);

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
  AddSharedGroup(/*owner=*/YES, self.testServer);

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
  AddSharedGroup(/*owner=*/NO, self.testServer);

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

// Checks last tab close alert as owner of the group open a new tab and close
// the last tab, when "Keep Group" is pressed and delete the group when "Delete
// Group" is pressed.
- (void)testLastTabClosedOwnerAlert {
  AddSharedGroup(/*owner=*/YES, self.testServer);

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];
  // Wait until the page has finished loading.
  [ChromeEarlGrey waitForPageToFinishLoading];

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

// Ensures the last tab alert is displayed when closing the last tab of a shared
// group. The tested tab close flow are:
// * Close from the tab group view using:
//     - Cross button
//     - Context menu and then 'Close Tab'
// * Close from the navigating view, long press on the tab grid icon.
- (void)testLastTabClosedAlerts {
  AddSharedGroup(/*owner=*/YES, self.testServer);
  [ChromeEarlGrey waitForMainTabCount:1];
  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Check that kSharedTabTitle tab cell is in the group.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      assertWithMatcher:grey_notNil()];

  // Close the tab by pressing the cross button and check the alert.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:KeepSharedConfirmationButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Cancel the action.
  [[EarlGrey selectElementWithMatcher:KeepSharedConfirmationButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Close the tab by using the context menu and check the alert.
  LongPressOn(TabGridCellAtIndex(0));
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                                IDS_IOS_CONTENT_CONTEXT_CLOSETAB),
                            grey_not(grey_accessibilityTrait(
                                UIAccessibilityTraitNotEnabled)),
                            nil)] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:KeepSharedConfirmationButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Cancel the action.
  [[EarlGrey selectElementWithMatcher:KeepSharedConfirmationButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Open the tab and try to close it with the tab grid icon context menu.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGridCellAtIndex(0)];
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  LongPressOn(chrome_test_util::ShowTabsButton());
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                                IDS_IOS_CONTENT_CONTEXT_CLOSETAB),
                            grey_not(grey_accessibilityTrait(
                                UIAccessibilityTraitNotEnabled)),
                            nil)] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:KeepSharedConfirmationButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Cancel the action.
  [[EarlGrey selectElementWithMatcher:KeepSharedConfirmationButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Ensures the last tab close alert as a member is displayed when the group is
// shared.
// TODO(crbug.com/460745987): Test is flaky.
- (void)FLAKY_testLastTabClosedMemberAlert {
  AddSharedGroup(/*owner=*/NO, self.testServer);

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

// Ensures the Recent Activity panel is showing the right information.
// TODO(crbug.com/460746048): Test is flaky.
- (void)FLAKY_testRecentActivity {
  AddSharedGroup(/*owner=*/NO, self.testServer);

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Open the Recent Activity.
  [[EarlGrey selectElementWithMatcher:TabGroupOverflowMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:RecentActivityButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTabGroupRecentActivityIdentifier)]
      assertWithMatcher:grey_not(grey_notVisible())];
}

// Ensures that a new tab is added in the shared group when the last tab is
// detached.
- (void)testLastTabMovedOutOfWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }
  if (@available(iOS 19.0, *)) {
    // TODO(crbug.com/427699033): Re-enable test on iOS 26.
    // Fails to interact with new window.
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.");
  }
  // Load regular tab 1 on the first window.
  AddSharedGroup(/*owner=*/NO, self.testServer);
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:0];

  // Open a second window.
  [ChromeEarlGrey openNewWindow];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [ChromeEarlGreyUI openTabGrid];

  // Create a local group in the second window.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_longPress()];
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
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGroupCreationView()];

  // Go back to the first window and move out the tab in the shared group.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:ContextMenuItemWithAccessibilityLabel(
                                   l10n_util::GetNSString(
                                       IDS_IOS_CONTENT_CONTEXT_MOVETABTOGROUP))]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];
  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabel(
                                          l10n_util::GetPluralNSStringF(
                                              IDS_IOS_TAB_GROUP_TABS_NUMBER,
                                              1))] performAction:grey_tap()];
  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:1];
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:0];

  // Verify that the tab group view is still displayed.
  [[EarlGrey selectElementWithMatcher:TabGroupViewTitle(kSharedGroupTitle)]
      assertWithMatcher:grey_notNil()];
}

// Ensures new tab is added when closing the last tab of a shared group.
// TODO(crbug.com/454567832): Re-enable this test.
- (void)FLAKY_testCloseLastTabInSharedGroup {
  AddSharedGroup(/*owner=*/NO, self.testServer);
  [ChromeEarlGrey waitForMainTabCount:1];

  id<GREYMatcher> sharedTabMatcher =
      grey_allOf(grey_kindOfClassName(@"GridCell"),
                 grey_accessibilityLabel(kSharedTabTitle),
                 grey_sufficientlyVisible(), nil);

  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Check that kSharedTabTitle tab cell is in the group.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:sharedTabMatcher];

  // Close the tab.
  [ChromeEarlGrey closeTabAtIndex:0];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Check that kSharedTabTitle tab cell is not in the group anymore.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:sharedTabMatcher];
}

// Ensures the last tab close alert works when the closed tab is not the active
// one and there is no other NTP tab, see crbug.com/419042071.
- (void)testNotActiveLastTabClosedAlert {
  AddSharedGroup(/*owner=*/NO, self.testServer);

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridNewTabButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGreyUI openTabGrid];

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Close the tab.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];
  // Tap on "Keep Group"
  [[EarlGrey selectElementWithMatcher:KeepSharedConfirmationButton()]
      performAction:grey_tap()];

  // Verify that the tab group view is still displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kTabGroupViewIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Check that kSharedTabTitle tab cell is in not in the group anymore.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(TabWithTitle(kSharedTabTitle),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  // Wait until the page has finished loading.
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Ensure the new tab is a new tab page.
  const GURL currentURL = [ChromeEarlGrey webStateVisibleURL];
  const GURL expectedURL(kChromeUINewTabURL);
  GREYAssertEqual(expectedURL, currentURL, @"Page navigated unexpectedly to %s",
                  currentURL.spec().c_str());

  [ChromeEarlGrey waitForMainTabCount:2];
}

// Ensures that closing the last tab in an incognito group works.
- (void)testCloseLastTabInIncognito {
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGreyUI openTabGrid];
  CreateTabGroupAtIndex(0, kGroup1Name, /*first_group=*/true);

  // Leave the TabGrid to be able to long press on the tab.
  [[EarlGrey selectElementWithMatcher:TabGridDoneButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_TOOLS_MENU_CLOSE_TAB)] performAction:grey_tap()];

  [ChromeEarlGrey waitForIncognitoTabCount:0];
}

// Ensures new tab is added when moving the last tab of a shared group.
// TODO(crbug.com/442448866): Re-enable this test.
- (void)FLAKY_testMoveLastTabInSharedGroup {
  // Create 2 groups, one shared and one local.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab2Title)];
  AddSharedGroup(/*owner=*/NO, self.testServer);
  CreateTabGroupAtIndex(0, kGroup2Name, /*first_group=*/false);

  // Open the shared group and move the only tab in it to the other group.
  OpenTabGroupAtIndex(1);
  LongPressOn(TabWithTitle(kSharedTabTitle));
  id<GREYMatcher> moveContextMenuButton = ContextMenuItemWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_MOVETABTOGROUP));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:moveContextMenuButton];
  [[EarlGrey selectElementWithMatcher:moveContextMenuButton]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabel(
                                          kGroup2Name)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabWithTitle(kSharedTabTitle)];

  // Verify that the shared tab group view is still displayed.
  [[EarlGrey selectElementWithMatcher:TabGroupViewTitle(kSharedGroupTitle)]
      assertWithMatcher:grey_notNil()];
  // Wait until the page has finished loading.
  [ChromeEarlGrey waitForPageToFinishLoading];
  // Make it active so we get the correct URL.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  // Verify that the new tab URL is chrome://newtab/.
  const GURL expectedURL(kChromeUINewTabURL);
  const GURL currentURL = [ChromeEarlGrey webStateVisibleURL];
  GREYAssertEqual(expectedURL, currentURL, @"Page navigated unexpectedly to %s",
                  currentURL.spec().c_str());
}

// Ensures new tab is added when moving the last tab of a shared group.
- (void)testLastTabCloseWithClearBrowsingData {
  AddSharedGroup(/*owner=*/NO, self.testServer);
  [ChromeEarlGrey waitForMainTabCount:1];

  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kSharedTabTitle)]
      performAction:grey_tap()];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GetQueryTitleURL(self.testServer, kTab1Title)];
  [ChromeEarlGrey waitForMainTabCount:2];

  // Open clear browsing data page.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityLabel(l10n_util::GetNSString(
                             IDS_IOS_TOOLS_MENU_CLEAR_BROWSING_DATA))];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                   l10n_util::GetNSString(
                                       IDS_IOS_DELETE_BROWSING_DATA_BUTTON))]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:TabGridGroupCellAtIndex(0)];
  [ChromeEarlGrey waitForMainTabCount:1];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];
  // Wait until the page has finished loading.
  [ChromeEarlGrey waitForPageToFinishLoading];
  // Check that the 2 tab cell open at the beginning of the test are not in the
  // group anymore.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kSharedTabTitle)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:TabWithTitle(kTab1Title)]
      assertWithMatcher:grey_nil()];
}

// Ensures that adding a tab from another account reflects correctly in a shared
// group.
// TODO(crbug.com/435327953): Reenable this test.
- (void)FLAKY_testAddNewTabFromAnotherAccount {
  AddSharedGroup(/*owner=*/YES, self.testServer);
  [ChromeEarlGrey waitForMainTabCount:1];

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Open a first tab and wait until loading is completed.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Add a new tab.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Make sure that the second tab exists in the group.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(1)]
      assertWithMatcher:grey_notNil()];

  // Sign out from the current identity (FakeIdentity1).
  [SigninEarlGrey signOut];

  // Open a new tab.
  [[EarlGrey selectElementWithMatcher:TabGridNewTabButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Sign in with another identity (FakeIdentity2).
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity2]
                         enableHistorySync:YES];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  [ChromeEarlGreyUI openTabGrid];
  [ChromeEarlGrey waitForMainTabCount:3];

  // Remove a new tab that is automatically created when a user signs in.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Verify that there are 2 tabs in the group.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(1)]
      assertWithMatcher:grey_notNil()];

  // Open a first tab and wait until loading is completed.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Add a new tab.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Sign out from the current identity (FakeIdentity2).
  [SigninEarlGrey signOut];
  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey selectElementWithMatcher:TabGridNewTabButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Sign in with the owner identity again (FakeIdentity1).
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                         enableHistorySync:YES];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  [ChromeEarlGreyUI openTabGrid];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Remove a new tab that is automatically created when a user signs in.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Verify that there are 3 tabs in the group.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(2)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the recent activity menu has a link to all activity logs.
- (void)testRecentActivityMenu {
  AddSharedGroup(/*owner=*/YES, self.testServer);
  [ChromeEarlGrey waitForMainTabCount:1];

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:TabGroupOverflowMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:RecentActivityButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kRecentActivityLogMenuButtonIdentifier)]
      performAction:grey_tap()];

  id<GREYMatcher> menuElement =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_SHARE_KIT_MANAGE_ACTIVITY_LOG_TITLE);
  [[EarlGrey selectElementWithMatcher:menuElement]
      assertWithMatcher:grey_notNil()];
  // Scope for the synchronization disabled.
  {
    // Disable synchronization to avoid network synchronization.
    ScopedSynchronizationDisabler syncDisabler;

    [[EarlGrey selectElementWithMatcher:menuElement] performAction:grey_tap()];

    ConditionBlock condition = ^{
      return
          [ChromeEarlGrey webStateVisibleURL] ==
          GURL(base::SysNSStringToUTF8([TabGroupAppInterface activityLogsURL]));
    };
    GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
               @"Wrong activity URL: %s instead of %@",
               [ChromeEarlGrey webStateVisibleURL].spec().c_str(),
               [TabGroupAppInterface activityLogsURL]);

    GREYAssertEqual(2UL, [ChromeEarlGrey mainTabCount],
                    @"Logs should be in new tab");
    [ChromeEarlGrey closeCurrentTab];
  }
  // End of the sync disabler scope.
}

// Tests that tapping items on Recent Activity takes an action corresponded to
// the item.
// TODO(crbug.com/440612088): This test is flaky.
- (void)FLAKY_testTapRecentActivityItems {
  AddSharedGroup(/*owner=*/YES, self.testServer);
  [ChromeEarlGrey waitForMainTabCount:1];

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Open a first tab and load https://example.com page.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [ChromeEarlGrey loadURL:GURL("https://example.com")];

  // Add a new tab.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Open the Recent Activity.
  [[EarlGrey selectElementWithMatcher:TabGroupOverflowMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:RecentActivityButton()]
      performAction:grey_tap()];

  // Verify that 2 items exist in the Recent Activity.
  [[EarlGrey selectElementWithMatcher:TabGroupRecentActivityCellAtIndex(0)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:TabGroupRecentActivityCellAtIndex(1)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the first item.
  [[EarlGrey selectElementWithMatcher:TabGroupRecentActivityCellAtIndex(0)]
      performAction:grey_tap()];

  // Verify that the newly added page (= new tab page) is open by tapping the
  // item in the Recent Activity.
  const GURL currentURL = [ChromeEarlGrey webStateVisibleURL];
  const GURL expectedURL(kChromeUINewTabURL);
  GREYAssertEqual(expectedURL, currentURL, @"Unexpected page %s is open",
                  currentURL.spec().c_str());

  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Close the first item.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];

  // Wait for one tab is closed.
  [ChromeEarlGrey waitForMainTabCount:1];

  // Open the Recent Activity.
  [[EarlGrey selectElementWithMatcher:TabGroupOverflowMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:RecentActivityButton()]
      performAction:grey_tap()];

  // Verify that 2 items exist in the Recent Activity.
  [[EarlGrey selectElementWithMatcher:TabGroupRecentActivityCellAtIndex(0)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:TabGroupRecentActivityCellAtIndex(1)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the first item to reopen the closed tab.
  [[EarlGrey selectElementWithMatcher:TabGroupRecentActivityCellAtIndex(0)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateVisible];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that the closed page is open again by tapping the item in the Recent
  // Activity.
  const GURL currentURL2 = [ChromeEarlGrey webStateVisibleURL];
  const GURL expectedURL2("https://example.com");
  GREYAssertEqual(expectedURL2, currentURL2, @"Unexpected page %s is open",
                  currentURL2.spec().c_str());

  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Verify that there are 2 tabs in the group.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(1)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the activity summary is displayed when a tab is added from sync to
// a shared tab group.
- (void)testActivitySummary {
#if TARGET_IPHONE_SIMULATOR
  // TODO(crbug.com/456719999): Re-enable the test on simulators.
  if (base::ios::IsRunningOnIOS26OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on simulators.");
  }
#endif

  AddSharedGroup(/*owner=*/YES, self.testServer);
  [ChromeEarlGrey waitForMainTabCount:1];

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Verify that 1 tab exists in the group.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Add tab to the shared group from sync.
  [TabGroupAppInterface addSharedTabToGroupAtIndex:0];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that the second tab is added.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGridCellAtIndex(1)];
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(1)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the activity summary is displayed.
  [[EarlGrey selectElementWithMatcher:TabGroupActivitySummaryCell()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the close button on the activity summary.
  [[EarlGrey selectElementWithMatcher:TabGroupActivitySummaryCellCloseButton()]
      performAction:grey_tap()];

  // Verify that the activity summary is not visible anymore.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGroupActivitySummaryCell()];
}

// Tests that the activity label on a group cell and a grid cell is updated when
// a shared group is updated.
- (void)testActivityLabel {
  // TODO(crbug.com/439552737): Re-enable the test on iOS26.
  if (base::ios::IsRunningOnIOS26OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.");
  }
#if !TARGET_IPHONE_SIMULATOR
  // TODO(crbug.com/449204815): Re-enable the test on iPad device.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
#endif

  AddSharedGroup(/*owner=*/YES, self.testServer);
  [ChromeEarlGrey waitForMainTabCount:1];

  // Add a tab to the shared group by a member in the shared group.
  [TabGroupAppInterface addSharedTabToGroupAtIndex:0];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that the activity label appears on the group cell.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      TabGroupActivityLabelOnGroupCellAtIndex(0)];
  [[EarlGrey
      selectElementWithMatcher:TabGroupActivityLabelOnGroupCellAtIndex(0)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Verify that the activity label appears on the grid cell.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      TabGroupActivityLabelOnGridCellAtIndex(1)];
  [[EarlGrey selectElementWithMatcher:TabGroupActivityLabelOnGridCellAtIndex(1)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open the tab added by a member in the shared group.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(1)]
      performAction:grey_tap()];

  // Go back to the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Verify that the activity label on the grid cell disappears.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      TabGroupActivityLabelOnGridCellAtIndex(1)];

  // Leave from the group view.
  [[EarlGrey selectElementWithMatcher:CloseTabGroupButton()]
      performAction:grey_tap()];

  // Verify that the activity label on the group cell disappears.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      TabGroupActivityLabelOnGroupCellAtIndex(0)];
}

// Tests that the badge on the tab switcher appears when a shared group is
// updated and disappears when a user visits the updated page.
- (void)testTabSwitcherBadge {
#if !TARGET_IPHONE_SIMULATOR
  // TODO(crbug.com/449204815): Re-enable the test on iPad device.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
#endif
  AddSharedGroup(/*owner=*/YES, self.testServer);
  [ChromeEarlGrey waitForMainTabCount:1];

  // Add a new tab.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridNewTabButton()]
      performAction:grey_tap()];

  // Add a tab to the shared group by a member in the shared group.
  [TabGroupAppInterface addSharedTabToGroupAtIndex:0];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that the badge on the tab switcher outside the group is visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:BlueDotOnShowTabsButton()];
  [[EarlGrey selectElementWithMatcher:BlueDotOnShowTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Go back to the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Open a first tab and wait until loading is completed.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that the badge on the tab switcher inside the group is visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:BlueDotOnShowTabsButton()];
  [[EarlGrey selectElementWithMatcher:BlueDotOnShowTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Go back to the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Open the tab added by a member in the shared group.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(1)]
      performAction:grey_tap()];

  // Verify that the badge on the tab switcher inside the group disappears.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:BlueDotOnShowTabsButton()];

  // Go back to the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Leave from the group view.
  [[EarlGrey selectElementWithMatcher:CloseTabGroupButton()]
      performAction:grey_tap()];

  // Open a tab outside the group.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(1)]
      performAction:grey_tap()];

  // Verify that the badge on the tab switcher outside the group disappears.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:BlueDotOnShowTabsButton()];
}

// Tests that the activity indicators (blue dot and notification dot) on the
// toolbar are updated when a shared group is updated.
// TODO(crbug.com/454262963): test is flaky, disable it.
- (void)DISABLED_testActivityIndicatorsOnToolbar {
  AddSharedGroup(/*owner=*/YES, self.testServer);
  [ChromeEarlGrey waitForMainTabCount:1];

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Open a first tab and wait until loading is completed.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Add a tab to the shared group by a member in the shared group.
  [TabGroupAppInterface addSharedTabToGroupAtIndex:0];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that the second tab is added.
  [ChromeEarlGrey waitForMainTabCount:2];

  // Verify that the badge on the show tabs button is visible.
  [[EarlGrey selectElementWithMatcher:BlueDotOnShowTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open a new tab outside of the group and check the dot is still visible.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:CloseTabGroupButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridNewTabButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BlueDotOnShowTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open the tab grid and create a group out of this tab.
  [ChromeEarlGreyUI openTabGrid];
  CreateTabGroupAtIndex(1, kGroup2Name, /*first_group=*/false);

  // Open the tab from the group and verify that the badge on the show tabs
  // button also disappears.
  [[EarlGrey selectElementWithMatcher:TabGridDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:BlueDotOnShowTabsButton()];
}

// Tests that the activity indicators (blue dot and notification dot) on the tab
// strip are updated when a shared group is updated.
- (void)testActivityIndicatorsOnTabStrip {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  AddSharedGroup(/*owner=*/YES, self.testServer);
  [ChromeEarlGrey waitForMainTabCount:1];

  // Open the group view.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Open a first tab and wait until loading is completed.
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Add a tab to the shared group by a member in the shared group.
  [TabGroupAppInterface addSharedTabToGroupAtIndex:0];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that the second tab is added.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripCellAtIndex(2)];
  [[EarlGrey selectElementWithMatcher:TabStripCellAtIndex(2)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the blue dot on the tab strip cell is visible, but the
  // notification dot on the group cell is not visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:BlueDotOnTabStripCellAtIndex(2)];
  [[EarlGrey selectElementWithMatcher:BlueDotOnTabStripCellAtIndex(2)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:NotificationDotOnTabStripGroupCellAtIndex(0)]
      assertWithMatcher:grey_notVisible()];

  // Verify that the badge on the show tabs button is also visible.
  [[EarlGrey selectElementWithMatcher:BlueDotOnShowTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Collapse the group.
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Verify that the notification dot on the tab strip group cell is visible,
  // but the blue dot on the tab cell is not visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      NotificationDotOnTabStripGroupCellAtIndex(0)];
  [[EarlGrey
      selectElementWithMatcher:NotificationDotOnTabStripGroupCellAtIndex(0)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:BlueDotOnTabStripCellAtIndex(2)]
      assertWithMatcher:grey_notVisible()];

  // Expand the group.
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Open the tab added by a member in the shared group.
  [[EarlGrey selectElementWithMatcher:TabStripCellAtIndex(2)]
      performAction:grey_tap()];

  // Verify that the blue dot on the tab strip cell disappears.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:BlueDotOnTabStripCellAtIndex(2)];

  // Verify that the badge on the show tabs button also disappears.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:BlueDotOnShowTabsButton()];

  // Collapse the group.
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellAtIndex(0)]
      performAction:grey_tap()];

  // Verify that the notification dot on the tab strip group cell disappears.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      NotificationDotOnTabStripGroupCellAtIndex(0)];
}

// Checks that a Share screen is dismissed upon signing out in another window.
- (void)testCancelShareGroupOnSignout {
  // Open a Share flow.
  [ChromeEarlGreyUI openTabGrid];
  CreateTabGroupAtIndex(0, kGroup1Name);
  LongPressTabGroupCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      performAction:grey_tap()];

  // Verify that it opened the Share flow.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:FakeShareFlowView()];

  // Sign out.
  [SigninEarlGrey signOut];

  // Verify that it closed the Share flow.
  [[EarlGrey selectElementWithMatcher:FakeShareFlowView()]
      assertWithMatcher:grey_notVisible()];
}

// Checks that a Share screen is dismissed if the primary account changes in
// another window.
- (void)testCancelShareGroupOnAccountSwitch {
  // Open a Share flow.
  [ChromeEarlGreyUI openTabGrid];
  CreateTabGroupAtIndex(0, kGroup1Name);
  LongPressTabGroupCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      performAction:grey_tap()];

  // Verify that it opened the Share flow.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:FakeShareFlowView()];

  // Sign in to another account.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity2]
              waitForSyncTransportActive:YES];

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
  return SharedTabGroupAppLaunchConfiguration(/*join_only=*/true);
}

- (void)setUp {
  [super setUp];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                         enableHistorySync:YES];

  // Make sure that the MessagingBackendService is fully initialized.
  NSError* error = [ChromeEarlGrey waitForMessagingBackendServiceInitialized];
  GREYAssertNil(error, @"Failed to initialize MessagingBackendService: %@",
                error);
}

- (void)tearDownHelper {
  [super tearDownHelper];
  // Delete all groups.
  [TabGroupAppInterface cleanup];
}

// Checks that the user with JoinOnly rights can't start the Share flow from
// Tab Grid.
- (void)testCantShareGroup {
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
  [TabGroupAppInterface mockSharedEntitiesPreview];
  GURL joinGroupURL = data_sharing::GetDataSharingUrl(data_sharing::GroupToken(
      data_sharing::GroupId("resources%2F3be"), "CggHBicxA_slvx"));
  [ChromeEarlGrey loadURL:joinGroupURL waitForCompletion:NO];

  WaitForFakeJoinFlowView();

  // Close the Join flow.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Verify that it closed the Join flow.
  [[EarlGrey selectElementWithMatcher:FakeJoinFlowView()]
      assertWithMatcher:grey_notVisible()];
}

// Checks that a Join screen is dismissed upon signing out in another window.
- (void)testCancelJoinGroupOnSignout {
  [TabGroupAppInterface mockSharedEntitiesPreview];
  GURL joinGroupURL = data_sharing::GetDataSharingUrl(data_sharing::GroupToken(
      data_sharing::GroupId("resources%2F3be"), "CggHBicxA_slvx"));
  [ChromeEarlGrey loadURL:joinGroupURL waitForCompletion:NO];

  WaitForFakeJoinFlowView();

  // Sign out.
  [SigninEarlGrey signOut];

  // Verify that it closed the Join flow.
  [[EarlGrey selectElementWithMatcher:FakeJoinFlowView()]
      assertWithMatcher:grey_notVisible()];
}

// Checks that a Join screen is dismissed if the primary account changes in
// another window.
- (void)testCancelJoinGroupOnAccountSwitch {
  [TabGroupAppInterface mockSharedEntitiesPreview];
  GURL joinGroupURL = data_sharing::GetDataSharingUrl(data_sharing::GroupToken(
      data_sharing::GroupId("resources%2F3be"), "CggHBicxA_slvx"));
  [ChromeEarlGrey loadURL:joinGroupURL waitForCompletion:NO];

  WaitForFakeJoinFlowView();

  // Sign in to another account.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity2]
              waitForSyncTransportActive:YES];

  [[EarlGrey selectElementWithMatcher:FakeJoinFlowView()]
      assertWithMatcher:grey_notVisible()];
}

@end
