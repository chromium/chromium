// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>
#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/new_tab_page_app_interface.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/constants.h"
#import "ios/chrome/browser/ui/push_notification/scoped_notification_auth_swizzler.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

namespace {

// Long presses the view with the given `accessibility_id`.
void LongPressView(NSString* accessibility_id) {
  id<GREYMatcher> matcher = grey_accessibilityID(accessibility_id);
  [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_longPress()];
}

// Wait for a view that contains a partial match to the given `text`, then tap
// it.
void WaitForThenTapText(NSString* text) {
  id item = chrome_test_util::ContainsPartialText(text);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:item];
  [[EarlGrey selectElementWithMatcher:item] performAction:grey_tap()];
}

// Taps a view containing a partial match to the given `text`.
void TapText(NSString* text) {
  id item = grey_allOf(chrome_test_util::ContainsPartialText(text),
                       grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:item] performAction:grey_tap()];
}

// Taps the context menu item with the given label.
void TapMenuItem(int labelId) {
  id item = chrome_test_util::ContextMenuItemWithAccessibilityLabelId(labelId);
  [[EarlGrey selectElementWithMatcher:item] performAction:grey_tap()];
}

}  // namespace

@interface PushNotificationTestCase : ChromeTestCase
@end

@implementation PushNotificationTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kIOSTipsNotifications);
  return config;
}

+ (void)setUpForTestCase {
  [super setUpForTestCase];

  [ChromeEarlGrey writeFirstRunSentinel];
  [ChromeEarlGrey clearDefaultBrowserPromoData];
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kIosDefaultBrowserPromoLastAction];
  [ChromeEarlGrey resetDataForLocalStatePref:
                      prefs::kIosCredentialProviderPromoLastActionTaken];
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kAppLevelPushNotificationPermissions];
  [NewTabPageAppInterface resetSetUpListPrefs];
}

- (void)tearDown {
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kAppLevelPushNotificationPermissions];
  [super tearDown];
}

// Tests that the settings page is dismissed by swiping down from the top.
- (void)testSetUpListMenuEnableNotifications {
  // Swizzle to grant notification auth when requested.
  ScopedNotificationAuthSwizzler auth(YES);

  // Long press the SetUpList module.
  LongPressView(set_up_list::kDefaultBrowserItemID);

  // Tap the menu item to enable notifications.
  TapText(@"Turn on notifications");

  // Tap the confirmation snackbar.
  WaitForThenTapText(@"notifications turned on");
}

- (void)testSetUpListMenuEnableNotificationsAfterDeniedCancel {
  // Swizzle in the "denied' auth status for notifications.
  ScopedNotificationAuthSwizzler auth(UNAuthorizationStatusDenied, NO);

  // Long press the SetUpList module.
  LongPressView(set_up_list::kDefaultBrowserItemID);

  // Tap the menu item to enable notifications.
  TapText(@"Turn on notifications");

  // Tap cancel action.
  TapMenuItem(IDS_IOS_NOTIFICATIONS_ALERT_CANCEL);
}

- (void)testSetUpListMenuEnableNotificationsAfterDeniedGoToSettings {
  // Swizzle in the "denied' auth status for notifications.
  ScopedNotificationAuthSwizzler auth(UNAuthorizationStatusDenied, NO);

  // Long press the SetUpList module.
  LongPressView(set_up_list::kDefaultBrowserItemID);

  // Tap the menu item to enable notifications.
  TapText(@"Turn on notifications");

  // Tap Go To Settings action.
  TapMenuItem(IDS_IOS_NOTIFICATIONS_ALERT_GO_TO_SETTINGS);

  // Verify that settings has opened, then close it.
  XCUIApplication* settingsApp = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.Preferences"];
  GREYAssertTrue([settingsApp waitForState:XCUIApplicationStateRunningForeground
                                   timeout:5],
                 @"The iOS Settings app should have opened.");
  [settingsApp terminate];

  // Reactivate the app.
  [[[XCUIApplication alloc] init] activate];
}

@end
