// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
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

// Matcher for the Reading List Text Badge.
id<GREYMatcher> ReadingListTextBadge() {
  NSString* new_overflow_menu_accessibility_id =
      [NSString stringWithFormat:@"%@-promoBadge", kToolsMenuReadingListId];
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

// Matcher for the TabPinned tip.
id<GREYMatcher> TabPinnedTip() {
  return grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_PINNED_TAB_OVERFLOW_ACTION_IPH_TEXT));
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

}  // namespace

// Tests related to the triggering of In Product Help features. Tests here
// should verify that the UI presents correctly once the help has been
// triggered. The feature engagement tracker Demo Mode feature can be used for
// this.
@interface FeatureEngagementTestCase : ChromeTestCase
@end

@implementation FeatureEngagementTestCase

- (void)enableDemoModeForFeature:(std::string)feature {
  std::string enable_features = base::StringPrintf(
      "%s:chosen_feature/%s", feature_engagement::kIPHDemoMode.name,
      feature.c_str());
  if ([self isRunningTest:@selector(testPinTabFromOverflowMenu)]) {
    enable_features += base::StringPrintf(",%s:%s/true", kEnablePinnedTabs.name,
                                          kEnablePinnedTabsOverflowParam);
  }
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.additional_args.push_back("--enable-features=" + enable_features);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

// Verifies that the Badged Reading List feature shows when triggering
// conditions are met. Also verifies that the Badged Reading List does not
// appear again after being shown.
- (void)testBadgedReadingListFeatureShouldShow {
  [self enableDemoModeForFeature:"IPH_BadgedReadingList"];

  [ChromeEarlGreyUI openToolsMenu];

  [[[EarlGrey selectElementWithMatcher:ReadingListTextBadge()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionRight, 150)
      onElementWithMatcher:grey_accessibilityID(kPopupMenuToolsMenuTableViewId)]
      assertWithMatcher:grey_notNil()];
}

// Verifies that the Badged Manual Translate Trigger feature shows.
- (void)testBadgedTranslateManualTriggerFeatureShows {
  if ([ChromeEarlGrey isNewOverflowMenuEnabled]) {
    // TODO(crbug.com/1285154): Reenable once this is supported.
    EARL_GREY_TEST_DISABLED(
        @"New overflow menu does not support translate badge");
  }

  [self enableDemoModeForFeature:"IPH_BadgedTranslateManualTrigger"];

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
}

// Verifies that the New Tab Tip appears when all conditions are met.
// TODO(crbug.com/934248) The test is flaky.
- (void)DISABLED_testNewTabTipPromoShouldShow {
  [self enableDemoModeForFeature:"IPH_NewTabTip"];

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
- (void)testNewTabTipPromoDoesNotAppearOnNTP {
  [self enableDemoModeForFeature:"IPH_NewTabTip"];

  // Open and close the tab switcher to potentially trigger the New Tab Tip.
  OpenAndCloseTabSwitcher();

  // Verify that the New Tab Tip did not appear.
  [[EarlGrey selectElementWithMatcher:NewTabTipBubble()]
      assertWithMatcher:grey_notVisible()];
}

// Verifies that the bottom toolbar tip is displayed when the phone is in split
// toolbar mode.
- (void)testBottomToolbarAppear {
  if (![ChromeEarlGrey isSplitToolbarMode])
    return;

  // The IPH appears immediately on startup, so don't open a new tab when the
  // app starts up.
  [[self class] testForStartup];

  // Scope for the synchronization disabled.
  {
    ScopedSynchronizationDisabler syncDisabler;

    [self enableDemoModeForFeature:"IPH_BottomToolbarTip"];

    // Verify that the Bottom toolbar Tip appeared.
    ConditionBlock condition = ^{
      NSError* error = nil;
      [[EarlGrey selectElementWithMatcher:BottomToolbarTipBubble()]
          assertWithMatcher:grey_sufficientlyVisible()
                      error:&error];
      return error == nil;
    };
    // The app relaunch (to enable a feature flag) may take a while, therefore
    // the timeout is extended to 15 seconds.
    GREYAssert(WaitUntilConditionOrTimeout(base::Seconds(15), condition),
               @"Waiting for the Bottom Toolbar tip to appear");
  }  // End of the sync disabler scope.
}

// Verifies that the bottom toolbar tip is not displayed when the phone is not
// in split toolbar mode.
- (void)testBottomToolbarDontAppearOnNonSplitToolbar {
  if ([ChromeEarlGrey isSplitToolbarMode])
    return;

  // The IPH appears immediately on startup, so don't open a new tab when the
  // app starts up.
  [[self class] testForStartup];

  [self enableDemoModeForFeature:"IPH_BottomToolbarTip"];

  // Verify that the Bottom toolbar Tip didn't appear.
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

  // The IPH appears immediately on startup, so don't open a new tab when the
  // app starts up.
  [[self class] testForStartup];

  [self enableDemoModeForFeature:"IPH_LongPressToolbarTip"];

  // Verify that the Long Press Tip appears now that the Bottom Toolbar tip has
  // been shown.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:LongPressTipBubble()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Waiting for the Long Press tip.");
}

// Verifies that the IPH for Request desktop shows when triggered
- (void)testRequestDesktopTip {
  [self enableDemoModeForFeature:"IPH_DefaultSiteView"];

  self.testServer->AddDefaultHandlers();

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");

  // Request the desktop version of a website to trigger the tip.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  RequestDesktopVersion();

  [[EarlGrey selectElementWithMatcher:DefaultSiteViewTip()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verifies that the IPH for Pinned tab is displayed after pinning a tab from
// the overflow menu.
- (void)testPinTabFromOverflowMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }
  if (@available(iOS 15, *)) {
  } else {
    // Only available for iOS 15+.
    return;
  }
  [self enableDemoModeForFeature:"IPH_TabPinnedFeature"];

  XCUIApplication* app = [[XCUIApplication alloc] init];

  // Make sure that the pinned tabs feature has never been used from the
  // overflow menu.
  [ChromeEarlGrey setUserDefaultObject:@(0) forKey:kPinnedTabsOverflowEntryKey];

  [ChromeEarlGreyUI openToolsMenu];

  // Check that the "N" IPH badge is displayed before tapping on the action.
  GREYAssert([[app images][@"overflowRowIPHBadgeIdentifier"] exists],
             @"The 'N' IPH bagde should be displayed.");
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuPinTabId)];

  NSString* pinTabSnackbarMessage =
      l10n_util::GetNSString(IDS_IOS_SNACKBAR_MESSAGE_PINNED_TAB);
  NSString* unpinTabSnackbarMessage =
      l10n_util::GetNSString(IDS_IOS_SNACKBAR_MESSAGE_UNPINNED_TAB);

  [[EarlGrey selectElementWithMatcher:TabPinnedTip()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(pinTabSnackbarMessage)]
      assertWithMatcher:grey_nil()];

  [ChromeEarlGreyUI openToolsMenu];

  // Check that the "N" IPH bagde is not displayed before tapping on the action.
  GREYAssertFalse([[app images][@"overflowRowIPHBadgeIdentifier"] exists],
                  @"The 'N' IPH bagde should not be displayed.");
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuUnpinTabId)];
  [[EarlGrey selectElementWithMatcher:TabPinnedTip()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(unpinTabSnackbarMessage)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Tap the snackbar to make it disappear.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(unpinTabSnackbarMessage)]
      performAction:grey_tap()];

  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuPinTabId)];
  [[EarlGrey selectElementWithMatcher:TabPinnedTip()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(pinTabSnackbarMessage)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Tap the snackbar to make it disappear.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(pinTabSnackbarMessage)]
      performAction:grey_tap()];
}

@end
