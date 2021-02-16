// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <map>
#import <string>

#include "base/ios/ios_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/history/history_ui_constants.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_app_interface.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/table_view/feature_flags.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO(crbug.com/1015113): The EG2 macro is breaking indexing for some reason
// without the trailing semicolon.  For now, disable the extra semi warning
// so Xcode indexing works for the egtest.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(RecentTabsAppInterface);
#pragma clang diagnostic pop

using chrome_test_util::RecentTabsMenuButton;

namespace {
const char kURLOfTestPage[] = "http://testPage";
const char kHTMLOfTestPage[] =
    "<head><title>TestPageTitle</title></head><body>hello</body>";
NSString* const kTitleOfTestPage = @"TestPageTitle";

// Makes sure at least one tab is opened and opens the recent tab panel.
void OpenRecentTabsPanel() {
  // At least one tab is needed to be able to open the recent tabs panel.
  if ([ChromeEarlGrey isIncognitoMode]) {
    if ([ChromeEarlGrey incognitoTabCount] == 0)
      [ChromeEarlGrey openNewIncognitoTab];
  } else {
    if ([ChromeEarlGrey mainTabCount] == 0)
      [ChromeEarlGrey openNewTab];
  }

  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:RecentTabsMenuButton()];
}

// Returns the matcher for the Recent Tabs table.
id<GREYMatcher> RecentTabsTable() {
  return grey_accessibilityID(
      kRecentTabsTableViewControllerAccessibilityIdentifier);
}

// Returns the matcher for the entry of the page in the recent tabs panel.
id<GREYMatcher> TitleOfTestPage() {
  return grey_allOf(
      grey_ancestor(RecentTabsTable()),
      chrome_test_util::StaticTextWithAccessibilityLabel(kTitleOfTestPage),
      grey_sufficientlyVisible(), nil);
}

GURL TestPageURL() {
  return web::test::HttpServer::MakeUrl(kURLOfTestPage);
}

}  // namespace

// Earl grey integration tests for Recent Tabs Panel Controller.
@interface RecentTabsTestCase : WebHttpServerChromeTestCase
@end

@implementation RecentTabsTestCase

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey clearBrowsingHistory];
  web::test::SetUpSimpleHttpServer(std::map<GURL, std::string>{{
      TestPageURL(),
      std::string(kHTMLOfTestPage),
  }});
  [RecentTabsAppInterface clearCollapsedListViewSectionStates];
}

// Tests that a closed tab appears in the Recent Tabs panel, and that tapping
// the entry in the Recent Tabs panel re-opens the closed tab.
- (void)testClosedTabAppearsInRecentTabsPanel {
  const GURL testPageURL = TestPageURL();

  // Open the test page in a new tab.
  [ChromeEarlGrey loadURL:testPageURL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello"];

  // Open the Recent Tabs panel, check that the test page is not
  // present.
  OpenRecentTabsPanel();
  [[EarlGrey selectElementWithMatcher:TitleOfTestPage()]
      assertWithMatcher:grey_nil()];
  [self closeRecentTabs];

  // Close the tab containing the test page and wait for the stack view to
  // appear.
  [ChromeEarlGrey closeCurrentTab];

  // Open the Recent Tabs panel and check that the test page is present.
  OpenRecentTabsPanel();
  [[EarlGrey selectElementWithMatcher:TitleOfTestPage()]
      assertWithMatcher:grey_notNil()];

  // Tap on the entry for the test page in the Recent Tabs panel and check that
  // a tab containing the test page was opened.
  [[EarlGrey selectElementWithMatcher:TitleOfTestPage()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(
                            testPageURL.GetContent())];
}

// Tests that tapping "Show Full History" open the history.
- (void)testOpenHistory {
  OpenRecentTabsPanel();

  // Tap "Show Full History"
  id<GREYMatcher> showHistoryMatcher =
      grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_HISTORY_SHOWFULLHISTORY_LINK),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:showHistoryMatcher]
      performAction:grey_tap()];

  // Make sure history is opened.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(
                                              l10n_util::GetNSString(
                                                  IDS_HISTORY_TITLE)),
                                          grey_accessibilityTrait(
                                              UIAccessibilityTraitHeader),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Close History.
  id<GREYMatcher> exitMatcher =
      grey_accessibilityID(kHistoryNavigationControllerDoneButtonIdentifier);
  [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];

  // Close tab.
  [ChromeEarlGrey closeCurrentTab];
}

// Tests that the sign-in promo can be reloaded correctly.
- (void)testRecentTabSigninPromoReloaded {
  OpenRecentTabsPanel();

  // Scroll to sign-in promo, if applicable.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Sign-in promo should be visible with no accounts on the device.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts
                           closeButton:NO];
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Sign-in promo should be visible with an account on the device.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount
                           closeButton:NO];
  [self closeRecentTabs];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
}

// Tests that the sign-in promo can be reloaded correctly while being hidden.
// crbug.com/776939
- (void)testRecentTabSigninPromoReloadedWhileHidden {
  OpenRecentTabsPanel();

  // Scroll to sign-in promo, if applicable
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts
                           closeButton:NO];

  // Tap on "Other Devices", to hide the sign-in promo.
  NSString* otherDevicesLabel =
      l10n_util::GetNSString(IDS_IOS_RECENT_TABS_OTHER_DEVICES);
  id<GREYMatcher> otherDevicesMatcher = grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabel(otherDevicesLabel),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:otherDevicesMatcher]
      performAction:grey_tap()];
  [SigninEarlGreyUI verifySigninPromoNotVisible];

  // Add an account.
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Tap on "Other Devices", to show the sign-in promo.
  [[EarlGrey selectElementWithMatcher:otherDevicesMatcher]
      performAction:grey_tap()];
  // Scroll to sign-in promo, if applicable
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount
                           closeButton:NO];
  [self closeRecentTabs];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
}

// Tests that the VC can be dismissed by swiping down.
- (void)testSwipeDownDismiss {
  // TODO(crbug.com/1129589): Test disabled on iOS14 iPhones.
  if (base::ios::IsRunningOnIOS14OrLater() && ![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails on iOS14 iPhones.");
  }
  if (!base::ios::IsRunningOnOrLater(13, 0, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 12 and lower.");
  }
  if (!IsCollectionsCardPresentationStyleEnabled()) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on when feature flag is off.");
  }
  OpenRecentTabsPanel();

  id<GREYMatcher> recentTabsViewController =
      grey_allOf(RecentTabsTable(), grey_sufficientlyVisible(), nil);

  // Check that the TableView is presented.
  [[EarlGrey selectElementWithMatcher:recentTabsViewController]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:recentTabsViewController]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the TableView has been dismissed.
  [[EarlGrey selectElementWithMatcher:recentTabsViewController]
      assertWithMatcher:grey_nil()];

  [ChromeEarlGrey closeCurrentTab];
}

// Tests that the Recent Tabs can be opened while signed in (prevent regression
// for https://crbug.com/1056613).
- (void)testOpenWhileSignedIn {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  OpenRecentTabsPanel();
}

// Tests that there is a text cell in the Recently Closed section when it's
// empty (Only with illustrated-empty-states flag enabled).
- (void)testRecentlyClosedEmptyState {
  OpenRecentTabsPanel();

  id<GREYInteraction> detailTextCell = [EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabelId(
                         IDS_IOS_RECENT_TABS_RECENTLY_CLOSED_EMPTY),
                     grey_sufficientlyVisible(), nil)];
  if ([ChromeEarlGrey isIllustratedEmptyStatesEnabled]) {
    [detailTextCell assertWithMatcher:grey_notNil()];
  } else {
    [detailTextCell assertWithMatcher:grey_nil()];
  }
}

// Tests that the Signin promo is visible in the Other Devices section
// (and with illustrated-empty-states enabled, there is the illustrated cell)
- (void)testOtherDevicesDefaultEmptyState {
  OpenRecentTabsPanel();

  id<GREYInteraction> illustratedCell = [EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(
                  kRecentTabsOtherDevicesIllustratedCellAccessibilityIdentifier),
              grey_sufficientlyVisible(), nil)];
  if ([ChromeEarlGrey isIllustratedEmptyStatesEnabled]) {
    [illustratedCell assertWithMatcher:grey_notNil()];
  } else {
    [illustratedCell assertWithMatcher:grey_nil()];
  }

  // Scroll to sign-in promo, if applicable
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts
                           closeButton:NO];
}

// Tests the Copy Link action on a recent tab's context menu.
- (void)testContextMenuCopyLink {
  if (![ChromeEarlGrey isNativeContextMenusEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Test disabled when Native Context Menus feature flag is off.");
  }

  [self loadTestURL];
  OpenRecentTabsPanel();
  [self longPressTestURLTab];

  GURL testURL = TestPageURL();
  [ChromeEarlGrey
      verifyCopyLinkActionWithText:[NSString stringWithUTF8String:testURL.spec()
                                                                      .c_str()]
                      useNewString:YES];
}

// Tests the Open in New Tab action on a recent tab's context menu.
- (void)testContextMenuOpenInNewTab {
  if (![ChromeEarlGrey isNativeContextMenusEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Test disabled when Native Context Menus feature flag is off.");
  }

  [self loadTestURL];
  OpenRecentTabsPanel();
  [self longPressTestURLTab];

  [ChromeEarlGrey verifyOpenInNewTabActionWithURL:TestPageURL().GetContent()];

  // Verify that Recent Tabs closed.
  [[EarlGrey selectElementWithMatcher:RecentTabsTable()]
      assertWithMatcher:grey_notVisible()];
}

// Tests the Open in New Window action on a recent tab's context menu.
- (void)testContextMenuOpenInNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  if (![ChromeEarlGrey isNativeContextMenusEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Test disabled when Native Context Menus feature flag is off.");
  }

  [self loadTestURL];
  OpenRecentTabsPanel();

  [self longPressTestURLTab];

  [ChromeEarlGrey verifyOpenInNewWindowActionWithContent:"hello"];

  // Validate that Recent tabs was not closed in the original window. The
  // Accessibility Element matcher is added as there are other (non-accessible)
  // recent tabs tables in each window's TabGrid (but hidden).
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_accessibilityElement(), nil)]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey closeAllExtraWindows];
}

// Tests the Share action on a recent tab's context menu.
- (void)testContextMenuShare {
  if (![ChromeEarlGrey isNativeContextMenusEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Test disabled when Native Context Menus feature flag is off.");
  }

  [self loadTestURL];
  OpenRecentTabsPanel();
  [self longPressTestURLTab];

  [ChromeEarlGrey verifyShareActionWithPageTitle:kTitleOfTestPage];
}

#pragma mark Helper Methods

// Opens a new tab and closes it, to make sure it appears as a recently closed
// tab.
- (void)loadTestURL {
  const GURL testPageURL = web::test::HttpServer::MakeUrl(kURLOfTestPage);

  // Open the test page in a new tab.
  [ChromeEarlGrey loadURL:testPageURL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello"];

  // Close the tab, making it appear in Recent Tabs.
  [ChromeEarlGrey closeCurrentTab];
}

// Long-presses on a recent tab entry.
- (void)longPressTestURLTab {
  // The test page may be there multiple times.
  [[[EarlGrey selectElementWithMatcher:TitleOfTestPage()] atIndex:0]
      performAction:grey_longPress()];
}

// Closes the recent tabs panel.
- (void)closeRecentTabs {
  id<GREYMatcher> exitMatcher =
      grey_accessibilityID(kTableViewNavigationDismissButtonId);
  [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];
  // Wait until the recent tabs panel is dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];
}

@end
