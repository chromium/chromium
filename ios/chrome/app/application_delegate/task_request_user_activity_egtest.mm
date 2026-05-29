// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "components/handoff/handoff_utility.h"
#import "ios/chrome/browser/bookmarks/public/bookmarks_ui_constants.h"
#import "ios/chrome/browser/bookmarks/test/bookmark_earl_grey.h"
#import "ios/chrome/browser/bookmarks/test/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/device_reauth/test/reauthentication_app_interface.h"
#import "ios/chrome/browser/history/ui_bundled/history_ui_constants.h"
#import "ios/chrome/browser/intents/model/intents_constants.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_app_interface.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_constants.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_egtest_utils.h"
#import "ios/chrome/browser/recent_tabs/public/recent_tabs_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

namespace {
const char exampleURL[] = "https://example.com";
}

// Tests the handling of user activity intents.
// These tests only check the behavior during a warm start. Cold start
// scenarios are not covered here.
@interface TaskRequestUserActivityTestCase : ChromeTestCase
@end

@implementation TaskRequestUserActivityTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kEnableNewStartupFlow);
  // Force the cached value in features.mm to be enabled by setting the user
  // default.
  config.additional_args.push_back("-IsEnableNewStartupFlowEnabled");
  config.additional_args.push_back("YES");
  return config;
}

// Tests that the "Open In Chrome" intent opens the specified URLs.
- (void)testOpenInChrome {
  GURL webURL = GURL(exampleURL);
  [ChromeEarlGrey
      sceneContinueUserActivityWithType:kSiriShortcutOpenInChrome
                                    url:base::SysUTF8ToNSString(webURL.spec())];

  [ChromeEarlGrey waitForWebStateVisibleURL:webURL];
  GREYAssertTrue([ChromeEarlGrey webStateVisibleURL] == webURL,
                 @"URL should be opened");
}

// Tests that the "Open In Incognito" intent opens the specified URLs in
// incognito.
- (void)testOpenInIncognito {
  GURL webURL = GURL(exampleURL);

  [ChromeEarlGrey
      sceneContinueUserActivityWithType:kSiriShortcutOpenInIncognito
                                    url:base::SysUTF8ToNSString(webURL.spec())];

  [ChromeEarlGrey waitForWebStateVisibleURL:webURL];
  GREYAssertTrue([ChromeEarlGrey webStateVisibleURL] == webURL,
                 @"URL should be opened");
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Should be in incognito mode");
}

// Tests that the "Add Bookmark To Chrome" intent adds the bookmark and shows a
// snackbar.
- (void)testAddBookmarkToChrome {
  GURL webURL = GURL(exampleURL);

  [BookmarkEarlGrey waitForBookmarkModelLoaded];

  [ChromeEarlGrey
      sceneContinueUserActivityWithType:kSiriShortcutAddBookmarkToChrome
                                    url:base::SysUTF8ToNSString(webURL.spec())];

  // Open bookmarks and verify that the URL was added.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  NSString* expectedTitle = base::SysUTF8ToNSString(webURL.host());

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TappableBookmarkNodeWithLabel(
                                   expectedTitle)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the "Add Reading List Item To Chrome" intent adds the item.
- (void)testAddReadingListItemToChrome {
  GURL webURL = GURL(exampleURL);

  // Ensure reading list model is loaded and empty.
  GREYAssertNil([ReadingListAppInterface clearEntries],
                @"Unable to clear Reading List entries");

  [ChromeEarlGrey
      sceneContinueUserActivityWithType:kSiriShortcutAddReadingListItemToChrome
                                    url:base::SysUTF8ToNSString(webURL.spec())];

  // Verify that the item was added to the model.
  GREYAssertEqual([ReadingListAppInterface unreadEntriesCount], 1,
                  @"Item was not added to reading list");

  // Open reading list and verify that the URL was added.
  reading_list_test_utils::OpenReadingList();

  // The title of the reading list item is constructed as the spec without
  // query/ref. For https://example.com, it becomes https://example.com/
  NSString* expectedTitle = @"https://example.com/";

  [[EarlGrey
      selectElementWithMatcher:reading_list_test_utils::VisibleReadingListItem(
                                   expectedTitle)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the "Open Reading List" intent opens the reading list.
- (void)testOpenReadingList {
  [ChromeEarlGrey sceneContinueUserActivityWithType:kSiriOpenReadingList
                                                url:nil];

  [ChromeEarlGrey waitForWebStateVisibleURL:GURL(kChromeUINewTabURL)];

  // Verify that the reading list is displayed.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                                          kReadingListViewID)];
}

// Tests that the "Open Bookmarks" intent opens the bookmarks.
- (void)testOpenBookmarks {
  [ChromeEarlGrey sceneContinueUserActivityWithType:kSiriOpenBookmarks url:nil];

  [ChromeEarlGrey waitForWebStateVisibleURL:GURL(kChromeUINewTabURL)];

  // Verify that the bookmarks UI is displayed.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(kBookmarksHomeTableViewIdentifier)];
}

// Tests that the "Open Recent Tabs" intent opens the recent tabs.
- (void)testOpenRecentTabs {
  [ChromeEarlGrey sceneContinueUserActivityWithType:kSiriOpenRecentTabs
                                                url:nil];

  [ChromeEarlGrey waitForWebStateVisibleURL:GURL(kChromeUINewTabURL)];

  // Verify that the recent tabs UI is displayed.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              kRecentTabsTableViewControllerAccessibilityIdentifier)];
}

// Tests that the "Open Tab Grid" intent opens the tab grid.
- (void)testOpenTabGrid {
  [ChromeEarlGrey sceneContinueUserActivityWithType:kSiriOpenTabGrid url:nil];

  // Verify that the tab grid is displayed.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:chrome_test_util::
                                                          TabGridDoneButton()];
}

// Tests that the "Open New Tab" intent opens a new tab.
- (void)testOpenNewTab {
  [ChromeEarlGrey sceneContinueUserActivityWithType:kSiriOpenNewTab url:nil];
  [ChromeEarlGrey waitForWebStateVisibleURL:GURL(kChromeUINewTabURL)];
}

// Tests that the "Play Dino Game" intent opens the dino game.
- (void)testPlayDinoGame {
  [ChromeEarlGrey sceneContinueUserActivityWithType:kSiriPlayDinoGame url:nil];
  [ChromeEarlGrey waitForWebStateVisibleURL:GURL(kChromeDinoGameURL)];
}

// Tests that the "Set Chrome Default Browser" intent opens the default browser
// settings.
- (void)testSetChromeDefaultBrowser {
  [ChromeEarlGrey sceneContinueUserActivityWithType:kSiriSetChromeDefaultBrowser
                                                url:nil];

  [ChromeEarlGrey waitForWebStateVisibleURL:GURL(kChromeUINewTabURL)];

  // Verify that the default browser settings are displayed.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(kDefaultBrowserSettingsTableViewId)];
}

// Tests that the "View History" intent opens the history UI.
- (void)testViewHistory {
  [ChromeEarlGrey sceneContinueUserActivityWithType:kSiriViewHistory url:nil];

  [ChromeEarlGrey waitForWebStateVisibleURL:GURL(kChromeUINewTabURL)];

  // Verify that the history UI is displayed.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              kHistoryTableViewIdentifier)];
}

// Tests that the "Open New Incognito Tab" intent opens a new incognito tab.
- (void)testOpenNewIncognitoTab {
  [ChromeEarlGrey sceneContinueUserActivityWithType:kSiriOpenNewIncognitoTab
                                                url:nil];

  [ChromeEarlGrey waitForWebStateVisibleURL:GURL(kChromeUINewTabURL)];

  // Verify that the current tab is incognito.
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Current tab is not incognito");
}

// Tests that the "Manage Payment Methods" intent opens the payment methods
// settings.
- (void)testManagePaymentMethods {
  [ChromeEarlGrey sceneContinueUserActivityWithType:kSiriManagePaymentMethods
                                                url:nil];

  [ChromeEarlGrey waitForWebStateVisibleURL:GURL(kChromeUINewTabURL)];

  // Verify that the payment methods settings are displayed.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              kAutofillCreditCardTableViewId)];
}

// Tests that the "Run Safety Check" intent opens the safety check UI.
- (void)testRunSafetyCheck {
  [ChromeEarlGrey sceneContinueUserActivityWithType:kSiriRunSafetyCheck
                                                url:nil];

  [ChromeEarlGrey waitForWebStateVisibleURL:GURL(kChromeUINewTabURL)];

  // Verify that the safety check UI is displayed.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              @"kSafetyCheckTableViewId")];
}

// Tests that the "Manage Passwords" intent opens the passwords settings.
- (void)testManagePasswords {
  [ReauthenticationAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  [ReauthenticationAppInterface mockReauthenticationModuleCanAttempt:YES];

  [ChromeEarlGrey sceneContinueUserActivityWithType:kSiriManagePasswords
                                                url:nil];

  [ChromeEarlGrey waitForWebStateVisibleURL:GURL(kChromeUINewTabURL)];

  // Verify that the passwords settings are displayed.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              kPasswordsTableViewID)];
}

// Tests that the "Manage Settings" intent opens the settings UI.
- (void)testManageSettings {
  [ChromeEarlGrey sceneContinueUserActivityWithType:kSiriManageSettings
                                                url:nil];

  [ChromeEarlGrey waitForWebStateVisibleURL:GURL(kChromeUINewTabURL)];

  // Verify that the settings UI is displayed.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              kSettingsTableViewId)];
}

// Tests that the "Clear Browsing Data" intent opens the Clear Browsing Data UI.
- (void)testClearBrowsingData {
  [ChromeEarlGrey sceneContinueUserActivityWithType:kSiriClearBrowsingData
                                                url:nil];

  [ChromeEarlGrey waitForWebStateVisibleURL:GURL(kChromeUINewTabURL)];

  // Verify that the Clear Browsing Data UI is displayed.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::ClearBrowsingDataView()];
}

// Tests that the Handoff intent opens the specified URLs.
- (void)testHandoffActivity {
  GURL webURL = GURL(exampleURL);
  [ChromeEarlGrey
      sceneContinueUserActivityWithType:handoff::kChromeHandoffActivityType
                                    url:base::SysUTF8ToNSString(webURL.spec())];

  [ChromeEarlGrey waitForWebStateVisibleURL:webURL];
  GREYAssertTrue([ChromeEarlGrey webStateVisibleURL] == webURL,
                 @"URL should be opened by Handoff");
}

@end
