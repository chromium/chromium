// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

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

// Matcher for the DefaultSiteView tip.
id<GREYMatcher> DefaultSiteViewTip() {
  return grey_accessibilityLabel(
      l10n_util::GetNSStringWithFixup(IDS_IOS_DEFAULT_PAGE_MODE_TIP));
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
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.iph_feature_enabled = feature;
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
    // TODO(crbug.com/40814816): Reenable once this is supported.
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

@end
