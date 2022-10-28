// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/feature_engagement/feature_engagement_app_interface.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/mac/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// The minimum number of times Chrome must be opened in order for the Reading
// List Badge to be shown.
const int kMinChromeOpensRequiredForReadingList = 5;

// The minimum number of times Chrome must be opened in order for the New Tab
// Tip to be shown.
const int kMinChromeOpensRequiredForNewTabTip = 3;

// URL path for a page with text in French.
const char kFrenchPageURLPath[] = "/french";

// Matcher for the Reading List Text Badge.
id<GREYMatcher> ReadingListTextBadge() {
  NSString* new_overflow_menu_accessibility_id =
      [NSString stringWithFormat:@"%@-badge", kToolsMenuReadingListId];
  return [ChromeEarlGrey isNewOverflowMenuEnabled]
             ? grey_accessibilityID(new_overflow_menu_accessibility_id)
             : grey_allOf(grey_accessibilityID(
                              @"kToolsMenuTextBadgeAccessibilityIdentifier"),
                          grey_ancestor(grey_allOf(
                              grey_accessibilityID(kToolsMenuReadingListId),
                              grey_sufficientlyVisible(), nil)),
                          nil);
}

// Matcher for the Translate Manual Trigger button.
id<GREYMatcher> TranslateManualTriggerButton() {
  return grey_allOf(grey_accessibilityID(kToolsMenuTranslateId),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the Translate Manual Trigger badge.
id<GREYMatcher> TranslateManualTriggerBadge() {
  return grey_allOf(
      grey_accessibilityID(@"kToolsMenuTextBadgeAccessibilityIdentifier"),
      grey_ancestor(TranslateManualTriggerButton()), nil);
}

// Matcher for the New Tab Tip Bubble.
id<GREYMatcher> NewTabTipBubble() {
  return grey_accessibilityLabel(
      l10n_util::GetNSStringWithFixup(IDS_IOS_NEW_TAB_IPH_PROMOTION_TEXT));
}

// Matcher for the Bottom Toolbar Tip Bubble.
id<GREYMatcher> BottomToolbarTipBubble() {
  return grey_accessibilityLabel(l10n_util::GetNSStringWithFixup(
      IDS_IOS_BOTTOM_TOOLBAR_IPH_PROMOTION_TEXT));
}

// Matcher for the Long Press Tip Bubble.
id<GREYMatcher> LongPressTipBubble() {
  return grey_accessibilityLabel(l10n_util::GetNSStringWithFixup(
      IDS_IOS_LONG_PRESS_TOOLBAR_IPH_PROMOTION_TEXT));
}

// Matcher for the DefaultSiteView tip.
id<GREYMatcher> DefaultSiteViewTip() {
  return grey_accessibilityLabel(
      l10n_util::GetNSStringWithFixup(IDS_IOS_DEFAULT_PAGE_MODE_TIP));
}

// Opens the TabGrid and then opens a new tab.
void OpenTabGridAndOpenTab() {
  [ChromeEarlGreyUI openTabGrid];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridNewTabButton()]
      performAction:grey_tap()];
}

// Opens and closes the tab switcher.
void OpenAndCloseTabSwitcher() {
  [ChromeEarlGreyUI openTabGrid];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Opens the tools menu and request the desktop version of the page.
void RequestDesktopVersion() {
  id<GREYMatcher> toolsMenuMatcher =
      [ChromeEarlGrey isNewOverflowMenuEnabled]
          ? grey_accessibilityID(kPopupMenuToolsMenuActionListId)
          : grey_accessibilityID(kPopupMenuToolsMenuTableViewId);

  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kToolsMenuRequestDesktopId),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:toolsMenuMatcher] performAction:grey_tap()];
}

// net::EmbeddedTestServer handler for kFrenchPageURLPath.
std::unique_ptr<net::test_server::HttpResponse> LoadFrenchPage(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_content_type("text/html");
  http_response->set_content(
      "Maître Corbeau, sur un arbre perché, Tenait en son bec un fromage. "
      "Maître Renard, par l’odeur alléché, Lui tint à peu près ce langage");
  return std::move(http_response);
}

}  // namespace

// Tests related to the triggering of In Product Help features.
@interface FeatureEngagementTestCase : ChromeTestCase
@end

@implementation FeatureEngagementTestCase

- (void)tearDown {
  [FeatureEngagementAppInterface reset];
  [super tearDown];
}

// Verifies that the Badged Reading List feature shows when triggering
// conditions are met. Also verifies that the Badged Reading List does not
// appear again after being shown.
- (void)testBadgedReadingListFeatureShouldShow {
  GREYAssert([FeatureEngagementAppInterface enableBadgedReadingListTriggering],
             @"Feature Engagement tracker did not load");

  // Ensure that Chrome has been launched enough times for the Badged Reading
  // List to appear.
  for (int index = 0; index < kMinChromeOpensRequiredForReadingList; index++) {
    [FeatureEngagementAppInterface simulateChromeOpenedEvent];
  }

  [ChromeEarlGreyUI openToolsMenu];

  [[[EarlGrey selectElementWithMatcher:ReadingListTextBadge()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:grey_accessibilityID(kPopupMenuToolsMenuTableViewId)]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGreyUI closeToolsMenu];

  // Reopen tools menu to verify that the badge does not appear again.
  [ChromeEarlGreyUI openToolsMenu];
  // Make sure the ReadingList entry is visible.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kToolsMenuReadingListId),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:grey_accessibilityID(kPopupMenuToolsMenuTableViewId)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:ReadingListTextBadge()]
      assertWithMatcher:grey_notVisible()];
}

// Verifies that the Badged Reading List feature does not show if Chrome has
// not opened enough times.
- (void)testBadgedReadingListFeatureTooFewChromeOpens {
  GREYAssert([FeatureEngagementAppInterface enableBadgedReadingListTriggering],
             @"Feature Engagement tracker did not load");

  // Open Chrome just one time.
  [FeatureEngagementAppInterface simulateChromeOpenedEvent];

  [ChromeEarlGreyUI openToolsMenu];

  [[EarlGrey selectElementWithMatcher:ReadingListTextBadge()]
      assertWithMatcher:grey_notVisible()];
}

// Verifies that the Badged Reading List feature does not show if the reading
// list has already been used.
- (void)testBadgedReadingListFeatureReadingListAlreadyUsed {
  GREYAssert([FeatureEngagementAppInterface enableBadgedReadingListTriggering],
             @"Feature Engagement tracker did not load");

  // Ensure that Chrome has been launched enough times to meet the trigger
  // condition.
  for (int index = 0; index < kMinChromeOpensRequiredForReadingList; index++) {
    [FeatureEngagementAppInterface simulateChromeOpenedEvent];
  }

  [FeatureEngagementAppInterface showReadingList];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewNavigationDismissButtonId)]
      performAction:grey_tap()];

  [ChromeEarlGreyUI openToolsMenu];

  [[EarlGrey selectElementWithMatcher:ReadingListTextBadge()]
      assertWithMatcher:grey_notVisible()];
}

// Verifies that the Badged Manual Translate Trigger feature shows only once
// when the triggering conditions are met.
- (void)testBadgedTranslateManualTriggerFeatureShouldShowOnce {
  if ([ChromeEarlGrey isNewOverflowMenuEnabled]) {
    // TODO(crbug.com/1285154): Reenable once this is supported.
    EARL_GREY_TEST_DISABLED(
        @"New overflow menu does not support translate badge");
  }
  GREYAssert([FeatureEngagementAppInterface enableBadgedTranslateManualTrigger],
             @"Feature Engagement tracker did not load");

  [ChromeEarlGreyUI openToolsMenu];

  // Make sure the Manual Translate Trigger entry is visible.
  [[[EarlGrey selectElementWithMatcher:TranslateManualTriggerButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_notNil()];

  // Make sure the Manual Translate Trigger entry badge is visible.
  [[[EarlGrey selectElementWithMatcher:TranslateManualTriggerBadge()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_notNil()];

  // Close tools menu by tapping reload.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::ReloadButton(),
                                   grey_ancestor(
                                       chrome_test_util::ToolsMenuView()),
                                   nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionUp, 150)
      onElementWithMatcher:chrome_test_util::ToolsMenuView()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI openToolsMenu];

  // Make sure the Manual Translate Trigger entry is visible.
  [[[EarlGrey selectElementWithMatcher:TranslateManualTriggerButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_notNil()];

  // Verify that the badge does not appear again.
  [[[EarlGrey selectElementWithMatcher:TranslateManualTriggerBadge()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_notVisible()];
}

// Verifies that the Badged Manual Translate Trigger feature does not show if
// the entry has already been used.
// TODO(crbug.com/1321264): This is failing flakily on several configurations.
- (void)DISABLED_testBadgedTranslateManualTriggerFeatureAlreadyUsed {
  // Set up the test server.
  self.testServer->RegisterDefaultHandler(base::BindRepeating(
      net::test_server::HandlePrefixedRequest, kFrenchPageURLPath,
      base::BindRepeating(&LoadFrenchPage)));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");

  // Load a URL with french text so that language detection is performed.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kFrenchPageURLPath)];

  GREYAssert([FeatureEngagementAppInterface enableBadgedTranslateManualTrigger],
             @"Feature Engagement tracker did not load");

  // Simulate using the Manual Translate Trigger entry.
  [FeatureEngagementAppInterface showTranslate];

  [ChromeEarlGreyUI openToolsMenu];

  // Make sure the Manual Translate Trigger entry is visible.
  [[[EarlGrey selectElementWithMatcher:TranslateManualTriggerButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_notNil()];

  // Verify that the badge does not appear.
  [[[EarlGrey selectElementWithMatcher:TranslateManualTriggerBadge()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_notVisible()];
}

// Verifies that the New Tab Tip appears when all conditions are met.
// TODO(crbug.com/934248) The test is flaky.
- (void)DISABLED_testNewTabTipPromoShouldShow {
  GREYAssert([FeatureEngagementAppInterface enableNewTabTipTriggering],
             @"Feature Engagement tracker did not load");

  // Ensure that Chrome has been launched enough times to meet the trigger
  // condition.
  for (int index = 0; index < kMinChromeOpensRequiredForNewTabTip; index++) {
    [FeatureEngagementAppInterface simulateChromeOpenedEvent];
  }

  // Navigate to a page other than the NTP to allow for the New Tab Tip to
  // appear.
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // Open and close the tab switcher to trigger the New Tab tip.
  OpenAndCloseTabSwitcher();

  // Verify that the New Tab Tip appeared.
  [[EarlGrey selectElementWithMatcher:NewTabTipBubble()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verifies that the New Tab Tip does not appear if all conditions are met,
// but the NTP is open.
// TODO(crbug.com/934248) The test is flaky.
- (void)DISABLED_testNewTabTipPromoDoesNotAppearOnNTP {
  GREYAssert([FeatureEngagementAppInterface enableNewTabTipTriggering],
             @"Feature Engagement tracker did not load");

  // Ensure that Chrome has been launched enough times to meet the trigger
  // condition.
  for (int index = 0; index < kMinChromeOpensRequiredForNewTabTip; index++) {
    [FeatureEngagementAppInterface simulateChromeOpenedEvent];
  }

  // Open and close the tab switcher to potentially trigger the New Tab Tip.
  OpenAndCloseTabSwitcher();

  // Verify that the New Tab Tip did not appear.
  [[EarlGrey selectElementWithMatcher:NewTabTipBubble()]
      assertWithMatcher:grey_notVisible()];
}

// Verifies that the bottom toolbar tip is displayed when the phone is in split
// toolbar mode.
// TODO(crbug.com/934248) The test is flaky.
- (void)DISABLED_testBottomToolbarAppear {
  if (![ChromeEarlGrey isSplitToolbarMode])
    return;

  GREYAssert([FeatureEngagementAppInterface enableBottomToolbarTipTriggering],
             @"Feature Engagement tracker did not load");

  // Open and close the tab switcher to potentially trigger the Bottom Toolbar
  // Tip.
  OpenAndCloseTabSwitcher();

  // Verify that the Bottom toolbar Tip appeared.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:BottomToolbarTipBubble()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Waiting for the Bottom Toolbar tip to appear");
}

// Verifies that the bottom toolbar tip is not displayed when the phone is not
// in split toolbar mode.
- (void)testBottomToolbarDontAppearOnNonSplitToolbar {
  if ([ChromeEarlGrey isSplitToolbarMode])
    return;

  GREYAssert([FeatureEngagementAppInterface enableBottomToolbarTipTriggering],
             @"Feature Engagement tracker did not load");

  // Open and close the tab switcher to potentially trigger the Bottom Toolbar
  // Tip.
  OpenAndCloseTabSwitcher();

  // Verify that the Bottom toolbar Tip appeared.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:BottomToolbarTipBubble()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(!WaitUntilConditionOrTimeout(base::Seconds(2), condition),
             @"The Bottom Toolbar tip shouldn't appear");
}

// Verifies that the LongPress tip is displayed only after the Bottom Toolbar
// tip is presented.
// TODO(crbug.com/934248) The test is flaky.
- (void)DISABLED_testLongPressTipAppearAfterBottomToolbar {
  if (![ChromeEarlGrey isSplitToolbarMode])
    return;

  GREYAssert([FeatureEngagementAppInterface enableLongPressTipTriggering],
             @"Feature Engagement tracker did not load");

  // Open the tab switcher and open a new tab to try to trigger the tip.
  OpenTabGridAndOpenTab();

  // Verify that the Long Press Tip don't appear if the bottom toolbar tip
  // hasn't been displayed.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:LongPressTipBubble()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(
      !WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
      @"The Long Press tip shouldn't appear before showing the other tip");

  // Enable the Bottom Toolbar tip.
  GREYAssert([FeatureEngagementAppInterface enableBottomToolbarTipTriggering],
             @"Feature Engagement tracker did not load");

  // Open the tab switcher and open a new tab to try to trigger the tip.
  OpenAndCloseTabSwitcher();

  // Verify that the Bottom Toolbar tip has been displayed.
  condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:BottomToolbarTipBubble()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Waiting for the Bottom Toolbar tip.");

  // Open the tab switcher and open a new tab to try to trigger the LongPress
  // tip.
  OpenTabGridAndOpenTab();

  // Verify that the Long Press Tip appears now that the Bottom Toolbar tip has
  // been shown.
  condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:LongPressTipBubble()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Waiting for the Long Press tip.");
}

// Verifies that the IPH for Request desktop is shown after 3 requests of the
// desktop version of a website.
- (void)testRequestDesktopTip {
  GREYAssert([FeatureEngagementAppInterface enableDefaultSiteViewTipTriggering],
             @"Feature Engagement tracker did not load");

  self.testServer->AddDefaultHandlers();

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");

  // Request the desktop version of a website, this should not trigger the tip.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  RequestDesktopVersion();

  [[EarlGrey selectElementWithMatcher:DefaultSiteViewTip()]
      assertWithMatcher:grey_nil()];

  // Second time, still no tip.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  RequestDesktopVersion();

  [[EarlGrey selectElementWithMatcher:DefaultSiteViewTip()]
      assertWithMatcher:grey_nil()];

  // Third time, this should trigger the tip.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  RequestDesktopVersion();

  [[EarlGrey selectElementWithMatcher:DefaultSiteViewTip()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Fourth time, the tip should no longer trigger.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  RequestDesktopVersion();

  [[EarlGrey selectElementWithMatcher:DefaultSiteViewTip()]
      assertWithMatcher:grey_nil()];
}

// Verifies that the IPH for Request desktop is not shown if the user interacted
// with the default page mode.
- (void)testRequestDesktopTipAfterChangingDefaultPageMode {
  GREYAssert([FeatureEngagementAppInterface enableDefaultSiteViewTipTriggering],
             @"Feature Engagement tracker did not load");

  [self togglePageMode];

  self.testServer->AddDefaultHandlers();

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");

  // Request the desktop version of a website, this should not trigger the tip.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  RequestDesktopVersion();

  [[EarlGrey selectElementWithMatcher:DefaultSiteViewTip()]
      assertWithMatcher:grey_nil()];

  // Second time, still no tip.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  RequestDesktopVersion();

  [[EarlGrey selectElementWithMatcher:DefaultSiteViewTip()]
      assertWithMatcher:grey_nil()];

  // Third time, the tip should still not be shown.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  RequestDesktopVersion();

  [[EarlGrey selectElementWithMatcher:DefaultSiteViewTip()]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Helpers

// Toggles the page mode from Mobile to Desktop and then back to Mobile.
- (void)togglePageMode {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::ContentSettingsButton()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_DEFAULT_PAGE_MODE_LABEL)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_DEFAULT_PAGE_MODE_DESKTOP)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabelId(
                         IDS_IOS_DEFAULT_PAGE_MODE_MOBILE),
                     grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
}

@end
