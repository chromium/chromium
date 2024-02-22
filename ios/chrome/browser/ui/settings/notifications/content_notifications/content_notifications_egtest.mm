// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::SettingsContentNotificationsTableView;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuNotificationsButton;

// Integration tests using the Content Notifications settings screen.
@interface ContentNotificationsTestCase : ChromeTestCase
@end

@implementation ContentNotificationsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Feature parameters follow a key/value format to enable or disable
  // parameters.
  std::string contentNotificationsFlag =
      std::string("ContentPushNotifications");
  std::string settingsMenuItem = std::string("NotificationSettingsMenuItem");

  config.additional_args.push_back("--enable-features=" +
                                   contentNotificationsFlag);

  return config;
}

// Tests that the settings page is dismissed by swiping down from the top.
- (void)testTrackingPriceSwipeDown {
  [self openContentNotificationsSettings];

  // Check that The Content TableView is presented.
  [[EarlGrey selectElementWithMatcher:SettingsContentNotificationsTableView()]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:SettingsContentNotificationsTableView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that Settings has been dismissed.
  [[EarlGrey selectElementWithMatcher:SettingsContentNotificationsTableView()]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Helpers

// Opens the content notifications settings from notification setting page.
- (void)openContentNotificationsSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuNotificationsButton()];
  [ChromeEarlGreyUI
      tapContentNotificationsMenuButton:
          ButtonWithAccessibilityLabelId(
              IDS_IOS_CONTENT_NOTIFICATIONS_CONTENT_SETTINGS_TOGGLE_TITLE)];
}

@end
