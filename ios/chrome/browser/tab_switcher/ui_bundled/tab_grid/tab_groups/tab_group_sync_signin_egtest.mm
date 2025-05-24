// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/test/ios/wait_util.h"
#import "base/threading/platform_thread.h"
#import "base/time/time.h"
#import "components/data_sharing/public/features.h"
#import "components/sync/base/command_line_switches.h"
#import "components/sync/base/data_type.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_eg_utils.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using chrome_test_util::CloseGroupButton;
using chrome_test_util::CreateTabGroupAtIndex;
using chrome_test_util::DeleteGroupButton;
using chrome_test_util::DeleteGroupConfirmationButton;
using chrome_test_util::TabGridCloseButtonForGroupCellAtIndex;
using chrome_test_util::TabGridGroupCellAtIndex;
using chrome_test_util::TabGridGroupCellWithName;
using chrome_test_util::TabGridOpenTabsPanelButton;
using chrome_test_util::TabGridTabGroupsPanelButton;
using chrome_test_util::TabGroupsPanelCellAtIndex;
using chrome_test_util::TabGroupsPanelCellWithName;

namespace {

// Put the number at the beginning to avoid issues with sentence case, as the
// keyboard default can differ iPhone vs iPad, simulator vs device.
NSString* const kGroup1Name = @"1group";
NSString* const kGroup2Name = @"2group";

// Constant for timeout while waiting for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Minutes(1);

// Deletes all saved groups in the Tab Groups panel. This function should be
// called in the Tab Groups panel.
void DeleteAllSavedGroups() {
  // It happens that on certain bots, the first grey_longPress action
  // doesn't return an error for EarlGrey, but the context menu doesn't open
  // accordingly. Waiting has been seen as fixing this.
  base::PlatformThread::Sleep(base::Seconds(1));
  // Delete the first tab group cell until there are none left over.
  NSError* error = nil;
  while (error == nil) {
    [[EarlGrey selectElementWithMatcher:TabGroupsPanelCellAtIndex(0)]
        performAction:grey_longPress()
                error:&error];
    if (!error) {
      [[EarlGrey selectElementWithMatcher:DeleteGroupButton()]
          performAction:grey_tap()];
      [[EarlGrey selectElementWithMatcher:DeleteGroupConfirmationButton()]
          performAction:grey_tap()];
    }
  }
}

// Waits for `entity_count` entities of type SAVED_TAB_GROUP on the fake server,
// and fails with a GREYAssert if the condition is not met, within a short
// period of time.
void WaitForEntitiesOnFakeServer(int entity_count) {
  syncer::DataType entity_type = syncer::SAVED_TAB_GROUP;
  ConditionBlock condition = ^{
    int count = [ChromeEarlGrey numberOfSyncEntitiesWithType:entity_type];
    return count == entity_count;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(kSyncOperationTimeout,
                                                          condition),
             @"Expected %d %s entities but found %d", entity_count,
             syncer::DataTypeToDebugString(entity_type),
             [ChromeEarlGrey numberOfSyncEntitiesWithType:entity_type]);
}

}  // namespace

@interface TabGroupSyncSignInTestCase : ChromeTestCase
@end

@implementation TabGroupSyncSignInTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabGroupSync);
  config.additional_args.push_back(std::string("--") +
                                   syncer::kSyncShortNudgeDelayForTest);
  config.features_enabled.push_back(
      data_sharing::features::kDataSharingFeature);
  return config;
}

// Tests that signing into an account with tab groups shows them in Tab Grid and
// Tab Groups panel.
- (void)testSignInWithGroupsAddsToTabGridAndTabGroupPanel {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  // Sign in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey setSelectedType:syncer::UserSelectableType::kTabs
                          enabled:YES];

  [ChromeEarlGreyUI openTabGrid];

  // Switch over to the third panel and delete existing saved groups.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];
  DeleteAllSavedGroups();

  // Navigate back to the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Check that the group with `kGroup1Name` exists in Tab Grid.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      assertWithMatcher:grey_notNil()];

  // Check that the group with `kGroup1Name` exists in the Tab Groups panel.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];

  // Wait for the Saved Tab Group entities to reach the sync server.
  WaitForEntitiesOnFakeServer(2);

  // Stop syncing tabs.
  [SigninEarlGrey setSelectedType:syncer::UserSelectableType::kTabs enabled:NO];

  // Sign out.
  [SigninEarlGrey signOut];

  // Check that the group with `kGroup1Name` no longer exists in the Tab Groups
  // panel.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_nil()];

  // Check that the group with `kGroup1Name` no longer exists in Tab Grid.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      assertWithMatcher:grey_nil()];

  // Sign back in.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Check that the group with `kGroup1Name` still doesn't exist in the
  // Tab Groups panel.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_nil()];

  // Check that the group with `kGroup1Name` still doesn't exist in Tab Grid.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(0)]
      assertWithMatcher:grey_nil()];

  // Start syncing tabs.
  [SigninEarlGrey setSelectedType:syncer::UserSelectableType::kTabs
                          enabled:YES];

  // Check that the group with `kGroup1Name` exists in Tab Grid.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:TabGridGroupCellAtIndex(0)];

  // Check that the group with `kGroup1Name` exists in the Tab Groups panel.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];

  // Clean up all saved groups.
  DeleteAllSavedGroups();
  [SigninEarlGrey signOut];
}

// Tests that signing out keeps groups created before syncing and deletes groups
// created since.
- (void)testSignOutKeepsPreviousGroupDeletesNewGroup {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  [ChromeEarlGreyUI openTabGrid];

  // Switch over to the third panel and delete existing saved groups.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];
  DeleteAllSavedGroups();

  // Navigate back to the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Sign in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey setSelectedType:syncer::UserSelectableType::kTabs
                          enabled:YES];

  // Open a second tab and create a new group with it inside.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openTabGrid];
  CreateTabGroupAtIndex(1, kGroup2Name, /*first_group=*/false);

  // Exit the Tab Grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Wait for the Saved Tab Group entities to reach the sync server.
  WaitForEntitiesOnFakeServer(4);

  // Sign out.
  [SigninEarlGrey signOut];

  // Switch over to the third panel of the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CloseTabGroupButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroup1Name` still exists.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
  // Check that the group with `kGroup2Name` no longer exists.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup2Name, 1)]
      assertWithMatcher:grey_nil()];

  // Sign in again to clean up all saved groups.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  DeleteAllSavedGroups();
  [SigninEarlGrey signOut];
}

// Tests that stopping syncing tabs keeps groups created before syncing and
// deletes groups created since.
// TODO(crbug.com/419246344): Test is flaky.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testStopSyncingTabsKeepsPreviousGroupDeletesNewGroup \
  DISABLED_testStopSyncingTabsKeepsPreviousGroupDeletesNewGroup
#else
#define MAYBE_testStopSyncingTabsKeepsPreviousGroupDeletesNewGroup \
  testStopSyncingTabsKeepsPreviousGroupDeletesNewGroup
#endif
- (void)MAYBE_testStopSyncingTabsKeepsPreviousGroupDeletesNewGroup {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  [ChromeEarlGreyUI openTabGrid];

  // Switch over to the third panel and delete existing saved groups.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];
  DeleteAllSavedGroups();

  // Navigate back to the tab grid.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Sign in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey setSelectedType:syncer::UserSelectableType::kTabs
                          enabled:YES];

  // Open a second tab and create a new group with it inside.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openTabGrid];
  CreateTabGroupAtIndex(1, kGroup2Name, /*first_group=*/false);

  // Exit the Tab Grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Wait for the Saved Tab Group entities to reach the sync server.
  WaitForEntitiesOnFakeServer(4);

  // Stop syncing tabs.
  [SigninEarlGrey setSelectedType:syncer::UserSelectableType::kTabs enabled:NO];

  // Switch over to the third panel of the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CloseTabGroupButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the group with `kGroup1Name` still exists.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
  // Check that the group with `kGroup2Name` no longer exists.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup2Name, 1)]
      assertWithMatcher:grey_nil()];

  // Sync tabs again to clean up all saved groups.
  [SigninEarlGrey setSelectedType:syncer::UserSelectableType::kTabs
                          enabled:YES];
  DeleteAllSavedGroups();
  [SigninEarlGrey signOut];
}

// Tests that tab groups don't get reopened after signing out and back in
- (void)testSignOutAndBackInDoesNotReopenGroups {
  // TODO(crbug.com/415554855): Test is flaky on iPad device from 18.2.
  if (@available(iOS 18, *)) {
    if ([ChromeEarlGrey isIPadIdiom]) {
      EARL_GREY_TEST_DISABLED(@"Disabled on iOS 18+ on iPad.");
    }
  }

  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }

  // Ensure that there are no tab groups initially.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];
  DeleteAllSavedGroups();
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];

  // Sign in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey setSelectedType:syncer::UserSelectableType::kTabs
                          enabled:YES];

  // Create two tab groups.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openTabGrid];
  CreateTabGroupAtIndex(0, kGroup1Name);
  CreateTabGroupAtIndex(1, kGroup2Name, /*first_group=*/false);

  // Wait for the Saved Tab Group entities to reach the sync server.
  WaitForEntitiesOnFakeServer(4);

  // Close the second group.
  [[EarlGrey selectElementWithMatcher:TabGridCloseButtonForGroupCellAtIndex(1)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(kGroup2Name, 1)]
      assertWithMatcher:grey_nil()];

  // Exit the Tab Grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Sign out.
  [SigninEarlGrey signOut];

  // Ensure both tab groups are gone.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup2Name, 1)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Sign back in.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Open the tab grid, and verify that only the first group is open.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellWithName(kGroup2Name, 1)]
      assertWithMatcher:grey_nil()];

  // Verify that both groups do exist again in the Tab Groups panel.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellWithName(kGroup2Name, 1)]
      assertWithMatcher:grey_notNil()];

  // Clean up all saved groups.
  DeleteAllSavedGroups();
  [SigninEarlGrey signOut];
}

@end
