// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_app_interface.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/test/query_title_server_util.h"
#import "ios/chrome/browser/ui/tab_switcher/test/tabs_egtest_util.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::AddToBookmarksButton;
using chrome_test_util::AddToReadingListButton;
using chrome_test_util::BackButton;
using chrome_test_util::CancelButton;
using chrome_test_util::CloseTabMenuButton;
using chrome_test_util::CopyActivityButton;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::ShareButton;
using chrome_test_util::TabGridSearchBar;
using chrome_test_util::TabGridSearchTabsButton;

namespace {

// Matcher for the Inactive Tabs button.
id<GREYMatcher> GetMatcherForInactiveTabsButton() {
  return grey_accessibilityID(kInactiveTabsButtonAccessibilityIdentifier);
}

// Matcher for the Inactive Tabs grid.
id<GREYMatcher> GetMatcherForInactiveTabsGrid() {
  return grey_accessibilityID(kInactiveTabGridIdentifier);
}

// Matcher for the Close All Inactive button in the Inactive Tabs grid.
id<GREYMatcher> GetMatcherForCloseAllInactiveButton() {
  return grey_accessibilityID(kInactiveTabGridCloseAllButtonIdentifier);
}

// Matcher for the Close All Inactive button in the Inactive Tabs grid.
id<GREYMatcher> GetMatcherForCloseAllAction() {
  NSString* closeAllActionTitle = l10n_util::GetNSString(
      IDS_IOS_INACTIVE_TABS_CLOSE_ALL_CONFIRMATION_OPTION);
  return chrome_test_util::AlertAction(closeAllActionTitle);
}

// Matcher for Inactive Tabs Settings link in the preamble of the Inactive Tabs
// grid.
id<GREYMatcher> GetMatcherForInactiveTabsSettingsLink() {
  return grey_allOf(
      // The link is within a grid view header of class
      // `InactiveTabsPreambleHeader`.
      grey_ancestor(grey_kindOfClassName(@"InactiveTabsPreambleHeader")),
      // UIKit instantiates a `UIAccessibilityLinkSubelement` for the link
      // element in the label with attributed string.
      grey_kindOfClassName(@"UIAccessibilityLinkSubelement"),
      grey_accessibilityTrait(UIAccessibilityTraitLink), nil);
}

// Matcher for Inactive Tabs Settings screen.
id<GREYMatcher> GetMatcherForInactiveTabsSettings() {
  return chrome_test_util::SettingsInactiveTabsTableView();
}

// Matcher for Inactive Tabs Setting' Disabled option.
id<GREYMatcher> GetMatcherForSettingsDisabledOption() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_OPTIONS_INACTIVE_TABS_DISABLED)),
                    grey_userInteractionEnabled(), nil);
}

// Matcher for Inactive Tabs User Education screen.
id<GREYMatcher> GetMatcherForInactiveTabsUserEducation() {
  return grey_accessibilityID(
      kInactiveTabsUserEducationAccessibilityIdentifier);
}

// Matcher for Inactive Tabs User Education screen's Done button.
id<GREYMatcher> GetMatcherForUserEducationDoneButton() {
  return grey_accessibilityID(
      kConfirmationAlertPrimaryActionAccessibilityIdentifier);
}

// Matcher for Inactive Tabs User Education screen's Settings button.
id<GREYMatcher> GetMatcherForUserEducationSettingsButton() {
  return grey_accessibilityID(
      kConfirmationAlertSecondaryActionAccessibilityIdentifier);
}

}  // namespace

// Tests related to the Inactive Tabs feature.
@interface InactiveTabsTestCase : ChromeTestCase
@end

@implementation InactiveTabsTestCase

- (void)setUp {
  [super setUp];
  [self setUpTestServer];

  // Just have an NTP, no other previous tabs.
  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];

  // Ensure that inactive tabs preference settings is set to its default state.
  [ChromeEarlGrey setIntegerValue:0
                forLocalStatePref:prefs::kInactiveTabsTimeThreshold];
  GREYAssertEqual(
      0,
      [ChromeEarlGrey localStateIntegerPref:prefs::kInactiveTabsTimeThreshold],
      @"Inactive tabs preference is not set to default value.");

  // Mark the User Education screen as already-seen by default.
  [ChromeEarlGrey setUserDefaultsObject:@YES
                                 forKey:kInactiveTabsUserEducationShownOnceKey];
}

// Sets up the EmbeddedTestServer as needed for tests.
- (void)setUpTestServer {
  RegisterQueryTitleHandler(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");
}

// Relaunches the app with Inactive Tabs still enabled.
- (void)relaunchAppWithInactiveTabsEnabled {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      "--enable-features=" + std::string(kTabInactivityThreshold.name) + ":" +
      kTabInactivityThresholdParameterName + "/" +
      kTabInactivityThresholdImmediateDemoParam);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

// Relaunches the app with Inactive Tabs disabled.
- (void)relaunchAppWithInactiveTabsDisabled {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

#pragma mark - Tests

// Checks that when Inactive Tabs is not enabled, tabs are not moved to Inactive
// Tabs.
- (void)testInactiveTabDisabled {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }

  // Create tabs.
  CreateRegularTabs(1, self.testServer);
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tab count should be 0");

  // Relaunch the app with Inactive Tabs disabled.
  [self relaunchAppWithInactiveTabsDisabled];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // The Inactive Tabs button should not be visible.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      assertWithMatcher:grey_notVisible()];

  // There should be no inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tab count should be 0");
}

// Checks that when Inactive Tabs is enabled and old tabs are found, the
// Inactive Tabs button appears in the Tab Grid.
- (void)testActiveTabsMoveToInactive {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }

  // Create tabs.
  CreateRegularTabs(1, self.testServer);
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tab count should be 0");

  // Relaunch the app.
  [self relaunchAppWithInactiveTabsEnabled];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // The Inactive Tabs button should be visible.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // There should be one inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 1,
                 @"Inactive tab count should be 1");
}

// Checks that when there are inactive tabs and the feature is disabled, the
// Inactive Tabs button no longer appears in the Tab Grid.
- (void)testInactiveTabsMoveToActiveWhenDisabling {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }

  // Create tabs.
  CreateRegularTabs(1, self.testServer);
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tab count should be 0");

  // Relaunch the app.
  [self relaunchAppWithInactiveTabsEnabled];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // The Inactive Tabs button should be visible.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // There should be one inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 1,
                 @"Inactive tab count should be 1");

  // Relaunch the app with Inactive Tabs disabled.
  [self relaunchAppWithInactiveTabsDisabled];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // The Inactive Tabs button should not be visible.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      assertWithMatcher:grey_notVisible()];

  // There should be no inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tab count should be 0");
}

// Checks that NTPs are not moved.
- (void)testActiveTabsDontMoveNTP {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];

  // Relaunch the app.
  [self relaunchAppWithInactiveTabsEnabled];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // The Inactive Tabs button should not be visible.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      assertWithMatcher:grey_notVisible()];

  // There should be no inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tab count should be 0");
}

// Checks that inactive tabs can be found with tab search.
- (void)testInactiveTabInTabSearch {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }

  // Create tabs with titles.
  CreateRegularTab(self.testServer, @"Tab1");
  CreateRegularTab(self.testServer, @"Tab2");
  CreateRegularTab(self.testServer, @"Tab3");

  // Relaunch the app.
  [self relaunchAppWithInactiveTabsEnabled];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Enter search mode.
  [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
      performAction:grey_tap()];

  // Search for the title of the 3rd inactive tab.
  [[EarlGrey selectElementWithMatcher:TabGridSearchBar()]
      performAction:grey_replaceText(@"Tab3")];

  // Check that the tab is here.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(@"Tab3")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that tapping on an inactive tab opens it.
- (void)testReactivateInactiveTab {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }
  CreateRegularTab(self.testServer, @"Tab1");
  [self relaunchAppWithInactiveTabsEnabled];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // There should be one inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 1,
                 @"Inactive tab count should be 1");

  // Enter the Inactive Tabs grid.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      performAction:grey_tap()];

  // Tap on the inactive tab.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(@"Tab1")]
      performAction:grey_tap()];

  // There should be no inactive tab anymore, it should have moved alongside the
  // initial NTP.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tab count should be 0");
}

// Checks that long-pressing on an inactive tab and closing it works as
// expected.
- (void)testCloseInactiveTabByLongPressing {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }
  CreateRegularTab(self.testServer, @"Tab1");
  [self relaunchAppWithInactiveTabsEnabled];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // There should be one inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 1,
                 @"Inactive tab count should be 1");

  // Enter the Inactive Tabs grid.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      performAction:grey_tap()];

  // Long press the tab.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(@"Tab1")]
      performAction:grey_longPress()];

  // Close the tab.
  [[EarlGrey selectElementWithMatcher:CloseTabMenuButton()]
      performAction:grey_tap()];

  // There should be no inactive tab anymore, just the initial NTP.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tab count should be 0");
}

// Checks tap on X symbols closes the inactive tab.
- (void)testCloseInactiveTabByCellCloseSymbol {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }
  CreateRegularTab(self.testServer, @"Tab1");
  [self relaunchAppWithInactiveTabsEnabled];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // There should be one inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 1,
                 @"Inactive tab count should be 1");

  // Enter the Inactive Tabs grid.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];

  // There should be no inactive tab anymore, just the initial NTP.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  [ChromeEarlGrey waitForInactiveTabCount:0];
}

// Checks that long-pressing on an inactive tab and sharing it opens the share
// sheet.
- (void)testShareInactiveTab {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }
  CreateRegularTab(self.testServer, @"Tab1");
  [self relaunchAppWithInactiveTabsEnabled];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // There should be one inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 1,
                 @"Inactive tab count should be 1");

  // Enter the Inactive Tabs grid.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      performAction:grey_tap()];

  // Long press the tab.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(@"Tab1")]
      performAction:grey_longPress()];

  // Share the tab.
  [[EarlGrey selectElementWithMatcher:ShareButton()] performAction:grey_tap()];

  // Check the presence of the Share sheet, for example by looking for the Copy
  // button.
  [ChromeEarlGrey tapButtonInActivitySheetWithID:@"Copy"];

  // There should be still be an inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 1,
                 @"Inactive tab count should be 1");
}

// Checks that long-pressing on an inactive tab and bookmarking it opens the
// "added bookmark" snackbar.
- (void)testBookmarkInactiveTab {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }
  CreateRegularTab(self.testServer, @"Tab1");
  [self relaunchAppWithInactiveTabsEnabled];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // There should be one inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 1,
                 @"Inactive tab count should be 1");

  // Enter the Inactive Tabs grid.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      performAction:grey_tap()];

  // Long press the tab.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(@"Tab1")]
      performAction:grey_longPress()];

  NSString* snackbarMessage = base::SysUTF16ToNSString(
      l10n_util::GetPluralStringFUTF16(IDS_IOS_BOOKMARKS_BULK_SAVED, 1));
  WaitForSnackbarTriggeredByTappingItem(snackbarMessage,
                                        AddToBookmarksButton());

  // There should be still be an inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 1,
                 @"Inactive tab count should be 1");
}

// Checks that long-pressing on an inactive tab and adding it to the Reading
// List opens the "added to Reading List" snackbar.
- (void)testAddToReadingListInactiveTab {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }
  CreateRegularTab(self.testServer, @"Tab1");
  [self relaunchAppWithInactiveTabsEnabled];
  // Clear the Reading List.
  GREYAssertNil([ReadingListAppInterface clearEntries],
                @"Unable to clear Reading List entries");
  GREYAssertEqual(0, [ReadingListAppInterface unreadEntriesCount],
                  @"Reading List should be empty");

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // There should be one inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 1,
                 @"Inactive tab count should be 1");

  // Enter the Inactive Tabs grid.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      performAction:grey_tap()];

  // Long press the tab.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(@"Tab1")]
      performAction:grey_longPress()];

  // Tap Add to Reading List.
  [[EarlGrey selectElementWithMatcher:AddToReadingListButton()]
      performAction:grey_tap()];

  // Check that the tab was added to Reading List.
  GREYAssertEqual(1, [ReadingListAppInterface unreadEntriesCount],
                  @"Reading List should have one element");

  // There should be still be an inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 1,
                 @"Inactive tab count should be 1");
}

// Checks that the Close All Inactive button and confirmation dialog work as
// expected.
- (void)testCloseAllInactiveTabs {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }

  // Create tabs.
  CreateRegularTabs(3, self.testServer);

  // Relaunch the app.
  [self relaunchAppWithInactiveTabsEnabled];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // There should be three inactive tabs.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 3,
                 @"Inactive tab count should be 3");

  // Enter the Inactive Tabs grid.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      performAction:grey_tap()];

  // Tab the Close All Inactive button.
  [[EarlGrey selectElementWithMatcher:GetMatcherForCloseAllInactiveButton()]
      performAction:grey_tap()];

  // Tap Cancel.
  [[EarlGrey selectElementWithMatcher:CancelButton()] performAction:grey_tap()];

  // There should still be three inactive tabs.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 3,
                 @"Inactive tab count should be 3");

  // Tab the Close All Inactive button again.
  [[EarlGrey selectElementWithMatcher:GetMatcherForCloseAllInactiveButton()]
      performAction:grey_tap()];

  // Tap Close All.
  [[EarlGrey selectElementWithMatcher:GetMatcherForCloseAllAction()]
      performAction:grey_tap()];

  // There should be no inactive tab anymore, just the initial NTP.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tab count should be 0");

  // The Inactive Tabs grid should no longer be visible.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsGrid()]
      assertWithMatcher:grey_notVisible()];

  // The Inactive Tabs button should not be visible.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      assertWithMatcher:grey_notVisible()];
}

// Checks that tapping the Settings link from the Inactive Tabs grid preamble
// opens Inactive Tabs Settings.
- (void)testSettingsFromPreamble {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }
  CreateRegularTabs(1, self.testServer);
  [self relaunchAppWithInactiveTabsEnabled];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // There should be one inactive tab, and the active NTP.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 1,
                 @"Inactive tab count should be 1");

  // Enter the Inactive Tabs grid.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      performAction:grey_tap()];

  // Tap on the settings link from the preamble.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsSettingsLink()]
      performAction:grey_tap()];

  // Check that Inactive Tabs Settings are open.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsSettings()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Dismiss the settings screen.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // The Inactive Tabs grid should be visible again.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsGrid()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::RegularTabGrid()]
      assertWithMatcher:grey_notVisible()];
}

// Checks that changing settings when presented from the Inactive Tabs grid
// updates the grid, and pops it when there are no inactive tabs anymore.
- (void)testSettingsChangesPopsInactiveTabs {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }
  CreateRegularTabs(1, self.testServer);
  [self relaunchAppWithInactiveTabsEnabled];
  [ChromeEarlGreyUI openTabGrid];

  // Enter the Inactive Tabs grid.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      performAction:grey_tap()];

  // There should be one inactive tab, and the active NTP.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 1,
                 @"Inactive tab count should be 1");

  // Tap on the settings link from the preamble.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsSettingsLink()]
      performAction:grey_tap()];

  // Disable Inactive Tabs and dismiss the settings screen.
  [[EarlGrey selectElementWithMatcher:GetMatcherForSettingsDisabledOption()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // The Inactive Tabs grid should no longer be visible.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsGrid()]
      assertWithMatcher:grey_notVisible()];

  // There should be no inactive tab, just 2 active tabs.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tab count should be 0");
}

// Checks that the count of inactive tabs appears.
- (void)testShowCount {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }
  CreateRegularTabs(3, self.testServer);

  [self relaunchAppWithInactiveTabsEnabled];

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // The Inactive Tabs count should be appended at the end of the button's
  // label.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      assertWithMatcher:grey_accessibilityLabel(
                            @"Inactive tabs, Tabs not used for 0 days, 3")];
}

// Checks that the User Education panel only appears the first time Inactive
// Tabs are opened.
- (void)testUserEducationAppearsOnce {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }
  // Reset the User-Education marker.
  [ChromeEarlGrey
      removeUserDefaultsObjectForKey:kInactiveTabsUserEducationShownOnceKey];

  // Set up one inactive tab.
  CreateRegularTabs(1, self.testServer);
  [self relaunchAppWithInactiveTabsEnabled];
  [ChromeEarlGreyUI openTabGrid];

  // Enter the Inactive Tabs grid.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      performAction:grey_tap()];

  // The user education screen is shown.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsUserEducation()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Dismiss it, go back and re-enter the Inactive Tabs grid.
  [[EarlGrey selectElementWithMatcher:GetMatcherForUserEducationDoneButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:testing::NavigationBarBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      performAction:grey_tap()];

  // The user education screen is not shown.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsUserEducation()]
      assertWithMatcher:grey_nil()];
}

// Checks that Settings can be opened from the User Education panel.
- (void)testUserEducationOpenSettings {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Inactive Tabs feature is "
                           @"only supported on iPhone.");
  }
  // Reset the User-Education marker.
  [ChromeEarlGrey
      removeUserDefaultsObjectForKey:kInactiveTabsUserEducationShownOnceKey];
  // Set up one inactive tab.
  CreateRegularTabs(1, self.testServer);
  [self relaunchAppWithInactiveTabsEnabled];
  [ChromeEarlGreyUI openTabGrid];

  // Enter the Inactive Tabs grid.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsButton()]
      performAction:grey_tap()];

  // The user education screen is shown.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsUserEducation()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Go to settings from that screen.
  [[EarlGrey
      selectElementWithMatcher:GetMatcherForUserEducationSettingsButton()]
      performAction:grey_tap()];

  // Check that Inactive Tabs Settings are open.
  [[EarlGrey selectElementWithMatcher:GetMatcherForInactiveTabsSettings()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
