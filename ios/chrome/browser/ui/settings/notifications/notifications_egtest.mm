// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>

#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/variations/pref_names.h"
#import "ios/chrome/browser/ui/push_notification/scoped_notification_auth_swizzler.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuNotificationsButton;
using chrome_test_util::SettingsNotificationsTableView;

namespace {

// Returns the matcher for the Tips Notifications switch.
id<GREYMatcher> TipsSwitchMatcher() {
  NSString* title = l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_TIPS_TITLE);
  return grey_accessibilityID([NSString stringWithFormat:@"%@, switch", title]);
}

// Returns the matcher for the Safety Check Notifications switch.
id<GREYMatcher> SafetyCheckSwitchMatcher() {
  NSString* title = l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_TITLE);
  return grey_accessibilityID([NSString stringWithFormat:@"%@, switch", title]);
}

// Taps the context menu item with the given label.
void TapMenuItem(int labelId) {
  id item = chrome_test_util::ContextMenuItemWithAccessibilityLabelId(labelId);
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
  // Feature parameters follow a key/value format to enable or disable
  // parameters.
  std::string params =
      ":enable_price_tracking/true/enable_price_notification/true";
  std::string priceNotificationsFlag =
      std::string(commerce::kCommercePriceTracking.name) + params;
  std::string shoppingListFlag = std::string("ShoppingList");
  std::string notificationMenuItemFlag =
      std::string("NotificationSettingsMenuItem");

  // Test the updated settings page when the Tips Notifications feature is
  // enabled.
  if ([self isRunningTest:@selector
            (testNotificationsSwipeDown_WithUpdatedSettingsView)] ||
      [self isRunningTest:@selector(testTipsSwitch)] ||
      [self isRunningTest:@selector(testSafetyCheckSwitch)]) {
    config.additional_args.push_back("--enable-features=IOSTipsNotifications");
    config.additional_args.push_back(
        "--enable-features=SafetyCheckNotifications");
  } else {
    config.additional_args.push_back("--disable-features=IOSTipsNotifications");
    config.additional_args.push_back(
        "--disable-features=SafetyCheckNotifications");
  }

  return config;
}

// Tests that the settings page is dismissed by swiping down from the top.
// TODO(crbug.com/326070899): remove this test when Tips Notifications is
// enabled by default.
- (void)testPriceNotificationsSwipeDown {
  // Price tracking might only be enabled in certain countries, so it is
  // overridden to ensure that it will be enabled.
  [ChromeEarlGrey setStringValue:"us"
               forLocalStatePref:variations::prefs::
                                     kVariationsPermanentOverriddenCountry];

  // Opens price notifications setting.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuNotificationsButton()];

  // Check that Price Notifications TableView is presented.
  [[EarlGrey selectElementWithMatcher:SettingsNotificationsTableView()]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:SettingsNotificationsTableView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that Settings has been dismissed.
  [[EarlGrey selectElementWithMatcher:SettingsNotificationsTableView()]
      assertWithMatcher:grey_nil()];
}

// Tests that the updated settings page is dismissed by swiping down from the
// top.
- (void)testNotificationsSwipeDown_WithUpdatedSettingsView {
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
  [[EarlGrey selectElementWithMatcher:TipsSwitchMatcher()]
      assertWithMatcher:grey_notNil()];

  // Toggle off the switch.
  [[EarlGrey selectElementWithMatcher:TipsSwitchMatcher()]
      performAction:grey_turnSwitchOn(NO)];

  // Toggle on the switch.
  [[EarlGrey selectElementWithMatcher:TipsSwitchMatcher()]
      performAction:grey_turnSwitchOn(YES)];

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
  [[EarlGrey selectElementWithMatcher:SafetyCheckSwitchMatcher()]
      assertWithMatcher:grey_notNil()];

  // Toggle off the switch.
  [[EarlGrey selectElementWithMatcher:SafetyCheckSwitchMatcher()]
      performAction:grey_turnSwitchOn(NO)];

  // Toggle on the switch.
  [[EarlGrey selectElementWithMatcher:SafetyCheckSwitchMatcher()]
      performAction:grey_turnSwitchOn(YES)];

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
