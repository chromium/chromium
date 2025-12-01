// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/public/group_data.h"
#import "components/data_sharing/test_support/test_utils.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
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

using chrome_test_util::DeleteSharedConfirmationButton;
using chrome_test_util::DeleteSharedGroupButton;
using chrome_test_util::LeaveSharedGroupButton;
using chrome_test_util::LeaveSharedGroupConfirmationButton;
using chrome_test_util::TabGridGroupCellAtIndex;
using chrome_test_util::TabGridTabGroupsPanelButton;
using chrome_test_util::TabGroupsPanelCellWithName;
using chrome_test_util::TabGroupsPanelNotificationCellAtIndex;

namespace {

NSString* const kGroupTitle = @"shared group";

// Adds a shared tab group with a test URL and sets the user as `owner` or not
// of the group.
void AddSharedGroup(BOOL owner,
                    net::test_server::EmbeddedTestServer* test_server) {
  NSString* url = base::SysUTF8ToNSString(
      GetQueryTitleURL(test_server, kGroupTitle).spec());
  [TabGroupAppInterface prepareFakeSharedTabGroups:1 asOwner:owner url:url];
  // Sleep for 3 seconds to make sure that the shared group data are correctly
  // fetched.
  // This sleep is longer than other `AddSharedGroup:` sleeps because, unlike
  // other shared group items, the panel group item directly contains its
  // sharing state. For other items, this state is fetched when long-pressing,
  // which delays the state check.
  base::PlatformThread::Sleep(base::Seconds(3));
  [ChromeEarlGreyUI openTabGrid];
  // Wait for the group cell to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::TabGridGroupCellAtIndex(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];
}

}  // namespace

// Tests for shared groups in the tab groups panel.
// Sync and sign-in tests are implemented in TabGroupSyncSignInTestCase.
@interface SharedTabGroupsPanelTestCase : ChromeTestCase
@end

@implementation SharedTabGroupsPanelTestCase

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

// Tests that deleting a shared tab group from groups panel works.
// TODO:(crbug.com/450935810): The test is flaky on simulator.
#if TARGET_OS_SIMULATOR
#define MAYBE_testSharedTabGroupsPanelDeleteSharedGroup \
  FLAKY_testSharedTabGroupsPanelDeleteSharedGroup
#else
#define MAYBE_testSharedTabGroupsPanelDeleteSharedGroup \
  testSharedTabGroupsPanelDeleteSharedGroup
#endif
- (void)MAYBE_testSharedTabGroupsPanelDeleteSharedGroup {
  AddSharedGroup(/*owner=*/YES, self.testServer);

  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroupTitle` exists.
  [[EarlGrey selectElementWithMatcher:TabGroupsPanelCellWithName(
                                          kGroupTitle, 1, /*shared=*/true)]
      assertWithMatcher:grey_notNil()];

  // Long press the group.
  [[EarlGrey selectElementWithMatcher:TabGroupsPanelCellWithName(
                                          kGroupTitle, 1, /*shared=*/true)]
      performAction:grey_longPress()];

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

// Tests that leaving a shared tab group from the tab groups panel works.
- (void)testSharedTabGroupsPanelLeaveSharedGroup {
  AddSharedGroup(/*owner=*/NO, self.testServer);

  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroupTitle` exists.
  [[EarlGrey selectElementWithMatcher:TabGroupsPanelCellWithName(
                                          kGroupTitle, 1, /*shared=*/true)]
      assertWithMatcher:grey_notNil()];

  // Long press the group.
  [[EarlGrey selectElementWithMatcher:TabGroupsPanelCellWithName(
                                          kGroupTitle, 1, /*shared=*/true)]
      performAction:grey_longPress()];

  // Verify that the delete button is not available.
  [[EarlGrey selectElementWithMatcher:DeleteSharedGroupButton()]
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

// Checks that being removed from a shared group makes a notification appear at
// the top of the Tab Groups panel.
// TODO(crbug.com/451982715): Test is flaky.
- (void)FLAKY_testNotificationOnSharedGroupRemoved {
  AddSharedGroup(/*owner=*/NO, self.testServer);
  [ChromeEarlGrey waitForMainTabCount:1];

  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroupTitle` exists.
  [[EarlGrey selectElementWithMatcher:TabGroupsPanelCellWithName(
                                          kGroupTitle, 1, /*shared=*/true)]
      assertWithMatcher:grey_notNil()];

  // Check that no notification is visible.
  [[EarlGrey selectElementWithMatcher:TabGroupsPanelNotificationCellAtIndex(0)]
      assertWithMatcher:grey_nil()];

  // Simulate the distant removal of the group.
  [TabGroupAppInterface removeAtIndex:0];

  // Check that the group has been deleted.
  GREYCondition* groupDeletedCheck =
      [GREYCondition conditionWithName:@"Wait for tab groups to be deleted"
                                 block:^{
                                   return [ChromeEarlGrey mainTabCount] == 0;
                                 }];
  bool groupDeleted = [groupDeletedCheck waitWithTimeout:10];
  GREYAssertTrue(groupDeleted, @"Failed to delete the shared group");

  // Check that the notification about the removal is visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:TabGroupsPanelNotificationCellAtIndex(
                                              0)];
  NSString* notificationText =
      l10n_util::GetNSStringF(IDS_COLLABORATION_ONE_GROUP_REMOVED_NOTIFICATION,
                              base::SysNSStringToUTF16(kGroupTitle));
  [[EarlGrey selectElementWithMatcher:grey_text(notificationText)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check closing the notification.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kTabGroupsPanelCloseNotificationIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGroupsPanelNotificationCellAtIndex(0)]
      assertWithMatcher:grey_nil()];
}

@end
