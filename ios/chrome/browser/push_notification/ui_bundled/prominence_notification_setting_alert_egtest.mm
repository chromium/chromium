// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>
#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/push_notification/ui_bundled/scoped_notification_auth_swizzler.h"
#import "ios/chrome/browser/settings/ui_bundled/notifications/notifications_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/earl_grey_scoped_block_swizzler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

namespace {

// Returns the matcher for the updated Notifications Settings screen.
id<GREYMatcher> NotificationsSettingsMatcher() {
  return grey_accessibilityID(kNotificationsBannerTableViewId);
}

// Taps the context menu item with the given label.
void TapMenuItem(int labelId) {
  id item = chrome_test_util::ContextMenuItemWithAccessibilityLabelId(labelId);
  [[EarlGrey selectElementWithMatcher:item] performAction:grey_tap()];
}

// Swizzles calls to `UIApplication` `openURL:` and allows inspecting the last
// opened url.
class ScopedOpenUrlSwizzler : public EarlGreyScopedBlockSwizzler {
 public:
  ScopedOpenUrlSwizzler()
      : EarlGreyScopedBlockSwizzler(@"UIApplication",
                                    @"openURL:options:completionHandler:",
                                    ^(id application,
                                      NSURL* url,
                                      NSDictionary* options,
                                      void (^completionHandler)(BOOL)) {
                                      this->opened_url_ = url;
                                      if (completionHandler) {
                                        completionHandler(YES);
                                      }
                                    }) {}

  // Returns the last opened url, or nil if `openURL:` has not been called.
  NSURL* opened_url() { return opened_url_; }

 private:
  NSURL* opened_url_;
};

}  // namespace

@interface ProminenceNotificationSettingAlertTestCase : ChromeTestCase
@end

@implementation ProminenceNotificationSettingAlertTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kProvisionalNotificationAlert);
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
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSettingsNotificationsTipsCellId, NO)]
      assertWithMatcher:grey_notNil()];

  // Toggle on the switch.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSettingsNotificationsTipsCellId, NO)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

  // Tap Go To Settings action.
  ScopedOpenUrlSwizzler swizzler;
  TapMenuItem(IDS_IOS_PROMINENCE_NOTIFICATION_SETTINGS_OPEN_SETTINGS_BUTTON);
  NSURL* expectedURL =
      [NSURL URLWithString:UIApplicationOpenNotificationSettingsURLString];
  GREYAssertEqualObjects(swizzler.opened_url(), expectedURL,
                         @"Expected Settings URL was not opened.");
}

@end
