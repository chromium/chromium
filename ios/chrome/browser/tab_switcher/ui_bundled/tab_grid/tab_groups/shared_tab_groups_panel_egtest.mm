// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/public/group_data.h"
#import "components/data_sharing/test_support/test_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_app_interface.h"
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

namespace {

NSString* const kGroupTitle = @"shared group";

// Adds a shared tab group. User's role depends on its fake identity.
void AddSharedGroup() {
  [TabGroupAppInterface prepareFakeSharedTabGroups:1];
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

  // `fakeIdentity2` joins shared groups as member.
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  if ([self
          isRunningTest:@selector(testSharedTabGroupsPanelDeleteSharedGroup)]) {
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

// Tests that deleting a shared tab group from groups panel works.
- (void)testSharedTabGroupsPanelDeleteSharedGroup {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  AddSharedGroup();

  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroupTitle` exists.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroupTitle, 1)]
      assertWithMatcher:grey_notNil()];

  // Long press the group.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroupTitle, 1)]
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

// Tests that leaving a shared tab group from groups panel works.
- (void)testSharedTabGroupsPanelLeaveSharedGroup {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  AddSharedGroup();

  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroupTitle` exists.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroupTitle, 1)]
      assertWithMatcher:grey_notNil()];

  // Long press the group.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroupTitle, 1)]
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

@end
