// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>

#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/push_notification/test/scoped_notification_auth_swizzler.h"
#import "ios/chrome/browser/settings/ui_bundled/notifications/content_notifications/content_notifications_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/notifications/notifications_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

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

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self isRunningTest:@selector(testReenterContentNotificationSettings)]) {
    config.features_enabled.push_back(kContentPushNotifications);
    config.features_enabled.push_back(
        kContentNotificationProvisionalIgnoreConditions);
  }
  return config;
}

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

// Tests that a user can go into the Content Notifications submenu, then go
// back, and then enter the submenu again without crashing.
- (void)testReenterContentNotificationSettings {
  // Sign in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Open notifications setting.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuNotificationsButton()];

  // Check that Notifications TableView is presented.
  [[EarlGrey selectElementWithMatcher:NotificationsSettingsMatcher()]
      assertWithMatcher:grey_notNil()];

  // Tap on Content Notifications menu button.
  id contentNotificationsCell = grey_allOf(
      chrome_test_util::ContainsPartialText(l10n_util::GetNSString(
          IDS_IOS_CONTENT_NOTIFICATIONS_CONTENT_SETTINGS_TOGGLE_TITLE)),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:contentNotificationsCell]
      performAction:grey_tap()];

  // Verify that the sub-menu is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kContentNotificationsTableViewId)]
      assertWithMatcher:grey_notNil()];

  // Tap back.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];

  // Verify that the sub-menu is fully gone before trying to re-enter.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kContentNotificationsTableViewId)]
      assertWithMatcher:grey_nil()];

  // Tap on Content Notifications menu button again.
  [[EarlGrey selectElementWithMatcher:contentNotificationsCell]
      performAction:grey_tap()];

  // Verify that the sub-menu is presented again.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kContentNotificationsTableViewId)]
      assertWithMatcher:grey_notNil()];
}

@end
