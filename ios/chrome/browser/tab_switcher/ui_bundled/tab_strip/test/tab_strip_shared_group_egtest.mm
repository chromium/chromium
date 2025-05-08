// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/public/group_data.h"
#import "components/data_sharing/test_support/test_utils.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_app_interface.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_strip/ui/swift_constants_for_objective_c.h"
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

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::DeleteSharedConfirmationButton;
using chrome_test_util::DeleteSharedGroupButton;
using chrome_test_util::KeepSharedConfirmationButton;
using chrome_test_util::LeaveSharedGroupButton;
using chrome_test_util::LeaveSharedGroupConfirmationButton;
using chrome_test_util::TabStripCellAtIndex;
using chrome_test_util::UngroupButton;

namespace {

NSString* const kGroupTitle = @"shared group";
NSString* const kSharedTabTitle = @"Google";

// Returns a matcher for a tab strip group cell with `title` as title.
id<GREYMatcher> TabStripGroupCellMatcher(NSString* title) {
  return grey_allOf(grey_kindOfClassName(@"UIView"),
                    grey_not(grey_kindOfClassName(@"UILabel")),
                    grey_accessibilityLabel(title),
                    grey_ancestor(grey_kindOfClassName(@"TabStripGroupCell")),
                    grey_sufficientlyVisible(), nil);
}

// Adds a shared tab group and sets the user as `owner` or not of the group.
void AddSharedGroup(BOOL owner) {
  [TabGroupAppInterface prepareFakeSharedTabGroups:1 asOwner:owner];
  // Sleep for 1 second to make sure that the shared group data are correctly
  // fetched.
  base::PlatformThread::Sleep(base::Seconds(1));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabStripGroupCellMatcher(
                                                          kGroupTitle)];
}

}  // namespace

// Tests for shared tab groups on the tab strip shown on iPad.
@interface TabStripSharedGroupTestCase : ChromeTestCase
@end

@implementation TabStripSharedGroupTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
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
  [ChromeEarlGrey
      setUserDefaultsObject:@YES
                     forKey:kSharedTabGroupUserEducationShownOnceKey];

  // `fakeIdentity2` joins shared groups as member.
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:identity enableHistorySync:YES];
}

- (void)tearDownHelper {
  [super tearDownHelper];
  // Delete all groups.
  [TabGroupAppInterface cleanup];
}

// Tests that deleting a shared tab group from tab strip works.
- (void)testTabStripSharedGroupDeleteSharedGroup {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }
  AddSharedGroup(/*owner=*/YES);

  // Long press the group.
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle)]
      performAction:grey_longPress()];

  // Verify that the leave and ungroup buttons are not available.
  [[EarlGrey selectElementWithMatcher:LeaveSharedGroupButton()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:UngroupButton()]
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
                                   return [ChromeEarlGrey mainTabCount] == 1;
                                 }];
  bool groupsDeleted = [groupsDeletedCheck waitWithTimeout:10];
  GREYAssertTrue(groupsDeleted, @"Failed to delete the shared group");
}

// Tests that leaving a shared tab group from tab strip works.
- (void)testTabStripSharedGroupLeaveSharedGroup {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }
  AddSharedGroup(/*owner=*/NO);

  // Long press the group.
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle)]
      performAction:grey_longPress()];

  // Verify that the delete and ungroup buttons are not available.
  [[EarlGrey selectElementWithMatcher:DeleteSharedGroupButton()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:UngroupButton()]
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
                                   return [ChromeEarlGrey mainTabCount] == 1;
                                 }];
  bool groupsLeaved = [groupsLeavedCheck waitWithTimeout:10];
  GREYAssertTrue(groupsLeaved, @"Failed to leave the shared group");
}

// Tests that when closing the last tab of shared group as an member of the
// group, an alert is displayed and works.
// TODO(crbug.com/415929742): Test fails on device.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testTabStripLastTabCloseInSharedGroupAlertAsMember \
  DISABLED_testTabStripLastTabCloseInSharedGroupAlertAsMember
#else
#define MAYBE_testTabStripLastTabCloseInSharedGroupAlertAsMember \
  testTabStripLastTabCloseInSharedGroupAlertAsMember
#endif
- (void)MAYBE_testTabStripLastTabCloseInSharedGroupAlertAsMember {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  AddSharedGroup(/*owner=*/NO);

  // Open the tab in shared group.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(kSharedTabTitle),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForMainTabCount:2];

  // Close the tab outside the group.
  [[EarlGrey selectElementWithMatcher:TabStripCellAtIndex(0)]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_CONTENT_CONTEXT_CLOSETAB)]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForMainTabCount:1];

  // Tap the close button of the current tab cell and verify the alert.
  id<GREYMatcher> currentTabCloseButtonMatcher = grey_allOf(
      grey_accessibilityID(
          TabStripTabItemConstants.closeButtonAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:currentTabCloseButtonMatcher]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteSharedConfirmationButton()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:LeaveSharedGroupConfirmationButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Tap on "Keep Group".
  [[EarlGrey selectElementWithMatcher:KeepSharedConfirmationButton()]
      performAction:grey_tap()];

  // Verify that the tab group cell is still displayed.
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle)]
      assertWithMatcher:grey_notNil()];

  // Wait until the page has finished loading.
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Ensure the new tab is a new tab page.
  const GURL currentURL = [ChromeEarlGrey webStateVisibleURL];
  const GURL expectedURL(kChromeUINewTabURL);
  GREYAssertEqual(expectedURL, currentURL, @"Page navigated unexpectedly to %s",
                  currentURL.spec().c_str());

  [ChromeEarlGrey waitForMainTabCount:1];

  // Close the tab and this time, leave the group.
  currentTabCloseButtonMatcher = grey_allOf(
      grey_accessibilityID(
          TabStripTabItemConstants.closeButtonAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:currentTabCloseButtonMatcher]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:KeepSharedConfirmationButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [ChromeEarlGrey waitForMainTabCount:1];
  [[EarlGrey selectElementWithMatcher:LeaveSharedGroupConfirmationButton()]
      performAction:grey_tap()];

  // Check that the group is deleted.
  [ChromeEarlGrey waitForMainTabCount:0];
}

// Tests that when closing the last tab of shared group as owner of the group,
// an alert is displayed and works.
// TODO(crbug.com/415929742): Test fails on device.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testTabStripLastTabCloseInSharedGroupAlertAsOwner \
  DISABLED_testTabStripLastTabCloseInSharedGroupAlertAsOwner
#else
#define MAYBE_testTabStripLastTabCloseInSharedGroupAlertAsOwner \
  testTabStripLastTabCloseInSharedGroupAlertAsOwner
#endif
- (void)MAYBE_testTabStripLastTabCloseInSharedGroupAlertAsOwner {
  if (@available(iOS 17, *)) {
  } else if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Only available on iOS 17+ on iPad.");
  }
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  AddSharedGroup(/*owner=*/YES);

  // Open the tab in shared group.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(kSharedTabTitle),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForMainTabCount:2];

  // Close the tab outside the group.
  [[EarlGrey selectElementWithMatcher:TabStripCellAtIndex(0)]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_CONTENT_CONTEXT_CLOSETAB)]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForMainTabCount:1];

  // Tap the close button of the current tab cell and verify the alert.
  id<GREYMatcher> currentTabCloseButtonMatcher = grey_allOf(
      grey_accessibilityID(
          TabStripTabItemConstants.closeButtonAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:currentTabCloseButtonMatcher]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteSharedConfirmationButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:LeaveSharedGroupConfirmationButton()]
      assertWithMatcher:grey_notVisible()];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Tap on "Keep Group".
  [[EarlGrey selectElementWithMatcher:KeepSharedConfirmationButton()]
      performAction:grey_tap()];

  // Verify that the tab group cell is still displayed.
  [[EarlGrey selectElementWithMatcher:TabStripGroupCellMatcher(kGroupTitle)]
      assertWithMatcher:grey_notNil()];

  // Wait until the page has finished loading.
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Ensure the new tab is a new tab page.
  const GURL currentURL = [ChromeEarlGrey webStateVisibleURL];
  const GURL expectedURL(kChromeUINewTabURL);
  GREYAssertEqual(expectedURL, currentURL, @"Page navigated unexpectedly to %s",
                  currentURL.spec().c_str());

  [ChromeEarlGrey waitForMainTabCount:1];

  // Close the tab and this time, leave the group.
  currentTabCloseButtonMatcher = grey_allOf(
      grey_accessibilityID(
          TabStripTabItemConstants.closeButtonAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:currentTabCloseButtonMatcher]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:KeepSharedConfirmationButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [ChromeEarlGrey waitForMainTabCount:1];
  [[EarlGrey selectElementWithMatcher:DeleteSharedConfirmationButton()]
      performAction:grey_tap()];

  // Check that the group is deleted.
  [ChromeEarlGrey waitForMainTabCount:0];
}

@end
