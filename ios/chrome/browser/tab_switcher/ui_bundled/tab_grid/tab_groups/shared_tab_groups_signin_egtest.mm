// Copyright 2025 The Chromium Authors
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
#import "components/signin/public/base/signin_pref_names.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/command_line_switches.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/test/signin_matchers.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_app_interface.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_eg_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/test/query_title_server_util.h"
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
using chrome_test_util::ButtonStackPrimaryButton;
using chrome_test_util::ConsistencySigninPrimaryButtonMatcher;
using chrome_test_util::CreateTabGroupAtIndex;
using chrome_test_util::FakeJoinFlowView;
using chrome_test_util::FakeShareFlowView;
using chrome_test_util::LongPressTabGroupCellAtIndex;
using chrome_test_util::ManageGroupButton;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::NavigationBarSaveButton;
using chrome_test_util::ShareGroupButton;
using chrome_test_util::TabGridGroupCellAtIndex;

namespace {

// Put the number at the beginning to avoid issues with sentence case, as the
// keyboard default can differ iPhone vs iPad, simulator vs device.
NSString* const kGroup1Name = @"1group";

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

// Returns the completely configured AppLaunchConfiguration (i.e. setting all
// the underlying feature dependencies), with the Shared Tab Groups flavor as a
// parameter.
AppLaunchConfiguration SharedTabGroupAppLaunchConfiguration(
    bool join_only = false) {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      collaboration::features::kCollaborationMessaging);
  config.features_enabled.push_back(
      data_sharing::features::kDataSharingFeature);
  config.features_disabled.push_back(kIOSAutoOpenRemoteTabGroupsSettings);

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
  [ChromeEarlGrey waitForMatcher:ConsistencySigninPrimaryButtonMatcher()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SIGNIN_GROUP_COLLABORATION_HALF_SHEET_SUBTITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Sign-in.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ConsistencySigninPrimaryButtonMatcher()]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI dismissSigninConfirmationSnackbarForIdentity:fakeIdentity
                                                   assertVisible:NO];

  // Check that a custom history & sync promo is displayed.
  [ChromeEarlGrey waitForMatcher:ButtonStackPrimaryButton()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_HISTORY_SYNC_GROUP_COLLABORATION_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Accept history & sync.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackPrimaryButton()]
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
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableHistorySync:NO];

  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // On iOS26 the grey_longPress action doesn't return an error for EarlGrey,
  // but the tab group doesn't open accordingly. Waiting has been seen as fixing
  // this.
  base::PlatformThread::Sleep(base::Seconds(1));

  // Share the first group.
  LongPressTabGroupCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      performAction:grey_tap()];

  // Check that a custom history & sync promo is displayed.
  [ChromeEarlGrey waitForMatcher:ButtonStackPrimaryButton()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_HISTORY_SYNC_GROUP_COLLABORATION_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Accept history & sync.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackPrimaryButton()]
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
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [TabGroupAppInterface mockSharedEntitiesPreview];
  GURL joinGroupURL = data_sharing::GetDataSharingUrl(data_sharing::GroupToken(
      data_sharing::GroupId("resources%2F3be"), "CggHBicxA_slvx"));
  [ChromeEarlGrey loadURL:joinGroupURL waitForCompletion:NO];

  // Check that a custom sign promo is displayed.
  [ChromeEarlGrey waitForMatcher:ButtonStackPrimaryButton()];
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_SIGNIN_GROUP_COLLABORATION_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SIGNIN_GROUP_COLLABORATION_SUBTITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Sign-in.
  [[EarlGrey selectElementWithMatcher:ButtonStackPrimaryButton()]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Check that a custom history & sync promo is displayed.
  [ChromeEarlGrey waitForMatcher:ButtonStackPrimaryButton()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_HISTORY_SYNC_GROUP_COLLABORATION_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Accept history & sync.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackPrimaryButton()]
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
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableHistorySync:NO];

  [TabGroupAppInterface mockSharedEntitiesPreview];
  GURL joinGroupURL = data_sharing::GetDataSharingUrl(data_sharing::GroupToken(
      data_sharing::GroupId("resources%2F3be"), "CggHBicxA_slvx"));
  [ChromeEarlGrey loadURL:joinGroupURL waitForCompletion:NO];

  // Check that a custom history & sync promo is displayed.
  [ChromeEarlGrey waitForMatcher:ButtonStackPrimaryButton()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_HISTORY_SYNC_GROUP_COLLABORATION_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Accept history & sync.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackPrimaryButton()]
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

// Tests joining a group when sign in is disabled.
- (void)testJoinGroupSignedInDisabled {
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kSigninAllowed];

  [TabGroupAppInterface mockSharedEntitiesPreview];
  GURL joinGroupURL = data_sharing::GetDataSharingUrl(data_sharing::GroupToken(
      data_sharing::GroupId("resources%2F3be"), "CggHBicxA_slvx"));
  [ChromeEarlGrey loadURL:joinGroupURL waitForCompletion:NO];

  // Check that a sign in disabled alert is presented.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      grey_text(l10n_util::GetNSString(
                          IDS_COLLABORATION_SIGNED_OUT_HEADER))];
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_COLLABORATION_SIGNED_OUT_BODY))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests sharing a group when sign in is disabled.
- (void)testShareGroupSignedInDisabled {
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kSigninAllowed];

  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  CreateTabGroupAtIndex(0, kGroup1Name);

  // Check that the share action is not available.
  LongPressTabGroupCellAtIndex(0);
  [[EarlGrey selectElementWithMatcher:ShareGroupButton()]
      assertWithMatcher:grey_notVisible()];
}
@end
