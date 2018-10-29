// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#import <map>
#import <string>

#include "base/test/scoped_feature_list.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/history/history_ui_constants.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
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
  if (chrome_test_util::GetMainTabCount() == 0)
    chrome_test_util::OpenNewTab();

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
  [ChromeEarlGrey clearBrowsingHistory];
  [super setUp];
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
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Tests that a closed tab appears in the Recent Tabs panel, and that tapping
// the entry in the Recent Tabs panel re-opens the closed tab.
- (void)testClosedTabAppearsInRecentTabsPanel {
  const GURL testPageURL = web::test::HttpServer::MakeUrl(kURLOfTestPage);

  // Open the test page in a new tab.
  [ChromeEarlGrey loadURL:testPageURL];
  [ChromeEarlGrey waitForWebViewContainingText:"hello"];

  // Open the Recent Tabs panel, check that the test page is not
  // present.
  OpenRecentTabsPanel();
  [[EarlGrey selectElementWithMatcher:TitleOfTestPage()]
      assertWithMatcher:grey_nil()];
  [self closeRecentTabs];

  // Close the tab containing the test page and wait for the stack view to
  // appear.
  chrome_test_util::CloseCurrentTab();
  // TODO(crbug.com/783192): ChromeEarlGrey should have a method to close the
  // current tab and synchronize with the UI.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

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
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_HISTORY_SHOWFULLHISTORY_LINK);
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
  chrome_test_util::CloseCurrentTab();
}

// Tests that the sign-in promo can be reloaded correctly.
- (void)testRecentTabSigninPromoReloaded {
  OpenRecentTabsPanel();
  // Sign-in promo should be visible with cold state.
  [SigninEarlGreyUI checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState
                                        closeButton:NO];
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  // Sign-in promo should be visible with warm state.
  [SigninEarlGreyUI checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState
                                        closeButton:NO];
  [self closeRecentTabs];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->RemoveIdentity(identity);
}

// Tests that the sign-in promo can be reloaded correctly while being hidden.
// crbug.com/776939
- (void)testRecentTabSigninPromoReloadedWhileHidden {
  OpenRecentTabsPanel();
  [SigninEarlGreyUI checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState
                                        closeButton:NO];

  // Tap on "Other Devices", to hide the sign-in promo.
  NSString* otherDevicesLabel =
      l10n_util::GetNSString(IDS_IOS_RECENT_TABS_OTHER_DEVICES);
  id<GREYMatcher> otherDevicesMatcher =
      chrome_test_util::ButtonWithAccessibilityLabel(otherDevicesLabel);
  [[EarlGrey selectElementWithMatcher:otherDevicesMatcher]
      performAction:grey_tap()];
  [SigninEarlGreyUI checkSigninPromoNotVisible];

  // Add an account.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Tap on "Other Devices", to show the sign-in promo.
  [[EarlGrey selectElementWithMatcher:otherDevicesMatcher]
      performAction:grey_tap()];
  [SigninEarlGreyUI checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState
                                        closeButton:NO];
  [self closeRecentTabs];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->RemoveIdentity(identity);
}

@end
