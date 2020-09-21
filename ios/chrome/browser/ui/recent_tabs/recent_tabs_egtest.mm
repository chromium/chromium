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
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/table_view/feature_flags.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

// Returns the matcher for the entry of the page in the recent tabs panel.
id<GREYMatcher> TitleOfTestPage() {
  return grey_allOf(
      grey_ancestor(grey_accessibilityID(
          kRecentTabsTableViewControllerAccessibilityIdentifier)),
      chrome_test_util::StaticTextWithAccessibilityLabel(kTitleOfTestPage),
      grey_sufficientlyVisible(), nil);
}

}  // namespace

// Earl grey integration tests for Recent Tabs Panel Controller.
@interface RecentTabsTestCase : ChromeTestCase
@end

@implementation RecentTabsTestCase

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey clearBrowsingHistory];
  web::test::SetUpSimpleHttpServer(std::map<GURL, std::string>{{
      web::test::HttpServer::MakeUrl(kURLOfTestPage),
      std::string(kHTMLOfTestPage),
  }});
  [NSUserDefaults.standardUserDefaults setObject:@{}
                                          forKey:kListModelCollapsedKey];
}

// Closes the recent tabs panel.
- (void)closeRecentTabs {
  id<GREYMatcher> exitMatcher =
      grey_accessibilityID(kTableViewNavigationDismissButtonId);
  [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];
  // Wait until the recent tabs panel is dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Tests that a closed tab appears in the Recent Tabs panel, and that tapping
// the entry in the Recent Tabs panel re-opens the closed tab.
- (void)testClosedTabAppearsInRecentTabsPanel {
  const GURL testPageURL = web::test::HttpServer::MakeUrl(kURLOfTestPage);

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
                                          nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

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
  // Sign-in promo should be visible with cold state.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeColdState
                           closeButton:NO];
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Sign-in promo should be visible with warm state.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeWarmState
                           closeButton:NO];
  [self closeRecentTabs];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
}

// Tests that the sign-in promo can be reloaded correctly while being hidden.
// crbug.com/776939
- (void)testRecentTabSigninPromoReloadedWhileHidden {
  OpenRecentTabsPanel();
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeColdState
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
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeWarmState
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
      grey_allOf(grey_accessibilityID(
                     kRecentTabsTableViewControllerAccessibilityIdentifier),
                 grey_sufficientlyVisible(), nil);

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
  if (base::FeatureList::IsEnabled(kIllustratedEmptyStates)) {
    [detailTextCell assertWithMatcher:grey_notNil()];
  } else {
    [detailTextCell assertWithMatcher:grey_nil()];
  }
}

// Test that the Cold Mode Signin promo is visible in the Other Devices section
// (and with illustrated-empty-states enabled, there is the illustrated cell)
- (void)testOtherDevicesDefaultEmptyState {
  OpenRecentTabsPanel();

  id<GREYInteraction> illustratedCell = [EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(
                  kRecentTabsOtherDevicesIllustratedCellAccessibilityIdentifier),
              grey_sufficientlyVisible(), nil)];
  if (base::FeatureList::IsEnabled(kIllustratedEmptyStates)) {
    [illustratedCell assertWithMatcher:grey_notNil()];
  } else {
    [illustratedCell assertWithMatcher:grey_nil()];
  }

  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeColdState
                           closeButton:NO];
}

@end
