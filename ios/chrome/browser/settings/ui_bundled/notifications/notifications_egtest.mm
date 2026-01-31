// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>

#import "ios/chrome/browser/push_notification/test/scoped_notification_auth_swizzler.h"
#import "ios/chrome/browser/settings/ui_bundled/notifications/notifications_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using chrome_test_util::SettingsMenuNotificationsButton;

namespace {

// Taps the alert item with the given label.
void TapAlertItem(int labelId) {
  id item = chrome_test_util::AlertItemWithAccessibilityLabelId(labelId);
  [[EarlGrey selectElementWithMatcher:item] performAction:grey_tap()];
}

// Returns the matcher for the updated Notifications Settings screen.
id<GREYMatcher> NotificationsSettingsMatcher() {
  return grey_accessibilityID(kNotificationsBannerTableViewId);
}

}  // namespace

// Integration tests using the Price Notifications settings screen.
@interface NotificationsTestCase : ChromeTestCase
@end

@implementation NotificationsTestCase

// Tests that the settings page is dismissed by swiping down from the top.
- (void)testPriceNotificationsSwipeDown {
  // Opens notifications setting.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuNotificationsButton()];

  // Check that Notifications TableView is presented.
  [[EarlGrey selectElementWithMatcher:NotificationsSettingsMatcher()]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:NotificationsSettingsMatcher()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that Settings has been dismissed.
  [[EarlGrey selectElementWithMatcher:NotificationsSettingsMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that switching on Tips Notifications on the updated settings page
// causes the alert prompt to appear when the user has disabled notifications.
- (void)testTipsSwitch {
  // Swizzle in the "denied' auth status for notifications.
  ScopedNotificationAuthSwizzler auth(UNAuthorizationStatusDenied, NO);
  // Opens notifications setting.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuNotificationsButton()];

  // Check that the TableView is presented.
  [[EarlGrey selectElementWithMatcher:NotificationsSettingsMatcher()]
      assertWithMatcher:grey_notNil()];

  // Toggle on the switch.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSettingsNotificationsTipsCellId, NO)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

  // Tap Go To Settings action.
  TapAlertItem(IDS_IOS_NOTIFICATIONS_ALERT_GO_TO_SETTINGS);

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

// Tests that switching on Safety Check Notifications on the updated settings
// page causes the alert prompt to appear when the user has disabled
// notifications.
- (void)testSafetyCheckSwitch {
  // Swizzle in the "denied' auth status for notifications.
  ScopedNotificationAuthSwizzler auth(UNAuthorizationStatusDenied, NO);
  // Opens notifications setting.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuNotificationsButton()];

  // Check that the TableView is presented.
  [[EarlGrey selectElementWithMatcher:NotificationsSettingsMatcher()]
      assertWithMatcher:grey_notNil()];

  // Toggle on the switch.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kSettingsNotificationsSafetyCheckCellId, NO)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

  // Tap Go To Settings action.
  TapAlertItem(IDS_IOS_NOTIFICATIONS_ALERT_GO_TO_SETTINGS);

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
