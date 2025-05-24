// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/collaboration/public/features.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/public/group_data.h"
#import "components/data_sharing/test_support/test_utils.h"
#import "components/sync/base/command_line_switches.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_matchers.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_app_interface.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_eg_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/test/query_title_server_util.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
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

using ::base::test::ios::kWaitForActionTimeout;
using chrome_test_util::CreateTabGroupAtIndex;
using chrome_test_util::FakeJoinFlowView;
using chrome_test_util::FakeShareFlowView;
using chrome_test_util::ManageGroupButton;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::NavigationBarSaveButton;
using chrome_test_util::PromoScreenPrimaryButtonMatcher;
using chrome_test_util::ShareGroupButton;
using chrome_test_util::TabGridGroupCellAtIndex;
using chrome_test_util::WebSigninPrimaryButtonMatcher;

namespace {

// Put the number at the beginning to avoid issues with sentence case, as the
// keyboard default can differ iPhone vs iPad, simulator vs device.
NSString* const kGroup1Name = @"1group";

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

  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Long press failed.");
}

// Waits for the fake join flow view to appear.
void WaitForFakeJoinFlowView() {
  GREYCondition* waitForFakeJoinFlowView = [GREYCondition
      conditionWithName:@"Wait for the fake join flow view to appear."
                  block:^{
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:FakeJoinFlowView()]
                        assertWithMatcher:grey_sufficientlyVisible()
                                    error:&error];
                    return error == nil;
                  }];
  GREYAssertTrue([waitForFakeJoinFlowView
                     waitWithTimeout:kWaitForActionTimeout.InSecondsF()],
                 @"The fake join flow view did not appear.");
}

// Long presses a tab group cell.
void LongPressTabGroupCellAtIndex(unsigned int index) {
  // Make sure the cell has appeared. Otherwise, long pressing can be flaky.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:TabGridGroupCellAtIndex(index)];
  LongPressOn(TabGridGroupCellAtIndex(index));
}

// Returns the completely configured AppLaunchConfiguration (i.e. setting all
// the underlying feature dependencies), with the Shared Tab Groups flavor as a
// parameter.
AppLaunchConfiguration SharedTabGroupAppLaunchConfiguration(
    bool join_only = false) {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabGroupSync);
  config.features_enabled.push_back(
      collaboration::features::kCollaborationMessaging);
  config.features_enabled.push_back(
      data_sharing::features::kDataSharingFeature);

  // Add the flag to use FakeTabGroupSyncService.
  config.additional_args.push_back(
      "--" + std::string(test_switches::kEnableFakeTabGroupSyncService));

  return config;
}

}  // namespace

// Test Shared Tab Groups sign in flows.
@interface SharedTabGroupsSigninTestCase : ChromeTestCase
@end

@implementation SharedTabGroupsSigninTestCase

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
}

- (void)tearDownHelper {
  [super tearDownHelper];
  // Delete all groups.
  [TabGroupAppInterface cleanup];
}

// Checks sharing a group without being signed in.
- (void)testShareGroupNotSignedIn {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Share the first group.
  LongPressTabGroupCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      performAction:grey_tap()];

  // Check that a custom sign promo is displayed.
  [ChromeEarlGrey waitForMatcher:WebSigninPrimaryButtonMatcher()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SIGNIN_GROUP_COLLABORATION_HALF_SHEET_SUBTITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Sign-in.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          WebSigninPrimaryButtonMatcher()]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Check that a custom history & sync promo is displayed.
  [ChromeEarlGrey waitForMatcher:PromoScreenPrimaryButtonMatcher()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_HISTORY_SYNC_GROUP_COLLABORATION_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Accept history & sync.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];
  GREYAssertTrue([ChromeEarlGrey isSyncHistoryDataTypeSelected],
                 @"History sync is disabled.");

  // Verify that this opened the fake Share flow.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:FakeShareFlowView()];
  [[EarlGrey selectElementWithMatcher:FakeShareFlowView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Share the group.
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
}

// Checks sharing a group without being synced.
- (void)testShareGroupNotSynced {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableHistorySync:NO];

  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Share the first group.
  LongPressTabGroupCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      performAction:grey_tap()];

  // Check that a custom history & sync promo is displayed.
  [ChromeEarlGrey waitForMatcher:PromoScreenPrimaryButtonMatcher()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_HISTORY_SYNC_GROUP_COLLABORATION_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Accept history & sync.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];
  GREYAssertTrue([ChromeEarlGrey isSyncHistoryDataTypeSelected],
                 @"History sync is disabled.");

  // Verify that this opened the fake Share flow.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:FakeShareFlowView()];
  [[EarlGrey selectElementWithMatcher:FakeShareFlowView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Share the group.
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
}

// Checks joining a group without being signed in.
- (void)testJoinGroupNotSignedIn {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [TabGroupAppInterface mockSharedEntitiesPreview];
  GURL joinGroupURL = data_sharing::GetDataSharingUrl(data_sharing::GroupToken(
      data_sharing::GroupId("resources%2F3be"), "CggHBicxA_slvx"));
  [ChromeEarlGrey loadURL:joinGroupURL waitForCompletion:NO];

  // Check that a custom sign promo is displayed.
  [ChromeEarlGrey waitForMatcher:PromoScreenPrimaryButtonMatcher()];
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_SIGNIN_GROUP_COLLABORATION_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SIGNIN_GROUP_COLLABORATION_SUBTITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Sign-in.
  [[EarlGrey selectElementWithMatcher:PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Check that a custom history & sync promo is displayed.
  [ChromeEarlGrey waitForMatcher:PromoScreenPrimaryButtonMatcher()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_HISTORY_SYNC_GROUP_COLLABORATION_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Accept history & sync.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];
  GREYAssertTrue([ChromeEarlGrey isSyncHistoryDataTypeSelected],
                 @"History sync is disabled.");

  // Verify that this opened the fake Join flow.
  WaitForFakeJoinFlowView();

  // Join the group.
  [[EarlGrey selectElementWithMatcher:NavigationBarSaveButton()]
      performAction:grey_tap()];

  // Verify that it closes the Share flow.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:FakeJoinFlowView()];

  // Verify that the shared group has been opened.
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Checks joining a group without being synced.
- (void)testJoinGroupNotSynced {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableHistorySync:NO];

  [TabGroupAppInterface mockSharedEntitiesPreview];
  GURL joinGroupURL = data_sharing::GetDataSharingUrl(data_sharing::GroupToken(
      data_sharing::GroupId("resources%2F3be"), "CggHBicxA_slvx"));
  [ChromeEarlGrey loadURL:joinGroupURL waitForCompletion:NO];

  // Check that a custom history & sync promo is displayed.
  [ChromeEarlGrey waitForMatcher:PromoScreenPrimaryButtonMatcher()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_HISTORY_SYNC_GROUP_COLLABORATION_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Accept history & sync.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];
  GREYAssertTrue([ChromeEarlGrey isSyncHistoryDataTypeSelected],
                 @"History sync is disabled.");

  // Verify that this opened the fake Join flow.
  WaitForFakeJoinFlowView();

  // Join the group.
  [[EarlGrey selectElementWithMatcher:NavigationBarSaveButton()]
      performAction:grey_tap()];

  // Verify that it closes the Share flow.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:FakeJoinFlowView()];

  // Verify that the shared group has been opened.
  [ChromeEarlGrey waitForMainTabCount:2];
}

@end
