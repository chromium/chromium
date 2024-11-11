// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>
#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/push_notification/scoped_notification_auth_swizzler.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
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

// Returns the matcher for the updated Notifications Settings screen.
id<GREYMatcher> NotificationsSettingsMatcher() {
  return grey_accessibilityID(kNotificationsBannerTableViewId);
}

// Returns the matcher for the Tips Notifications switch.
id<GREYMatcher> TipsSwitchMatcher() {
  NSString* title = l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_TIPS_TITLE);
  return grey_accessibilityID([NSString stringWithFormat:@"%@, switch", title]);
}

// Taps the context menu item with the given label.
void TapMenuItem(int labelId) {
  id item = chrome_test_util::ContextMenuItemWithAccessibilityLabelId(labelId);
  [[EarlGrey selectElementWithMatcher:item] performAction:grey_tap()];
}

}  // namespace

@interface ProminenceNotificationSettingAlertTestCase : ChromeTestCase
@end

@implementation ProminenceNotificationSettingAlertTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kProvisionalNotificationAlert);
  config.features_enabled.push_back(kIOSTipsNotifications);
  return config;
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kAppLevelPushNotificationPermissions];
}

- (void)tearDownHelper {
  [ChromeEarlGrey resetDataForLocalStatePref:
                      prefs::kProminenceNotificationAlertImpressionCount];
  [super tearDownHelper];
}

// Tests that the prominence alert displays upon toggling a feature notification
// setting when provisonal authorization is granted.
- (void)testProminenceNotificationSettingAlertShows {
  // Swizzle in the "provisional' auth status for notifications.
  ScopedNotificationAuthSwizzler auth(UNAuthorizationStatusProvisional, YES);

  // Opens price notifications setting.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:
                        chrome_test_util::SettingsMenuNotificationsButton()];

  // Check that the TableView is presented.
  [[EarlGrey selectElementWithMatcher:NotificationsSettingsMatcher()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TipsSwitchMatcher()]
      assertWithMatcher:grey_notNil()];

  // Toggle on the switch.
  [[EarlGrey selectElementWithMatcher:TipsSwitchMatcher()]
      performAction:grey_turnSwitchOn(YES)];

  // Tap Go To Settings action.
  TapMenuItem(IDS_IOS_PROMINENCE_NOTIFICATION_SETTINGS_OPEN_SETTINGS_BUTTON);

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
