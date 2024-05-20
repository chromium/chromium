// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/commerce/core/commerce_feature_list.h"
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
using chrome_test_util::SettingsTrackingPriceTableView;

// Integration tests using the Tracking Price settings screen.
@interface TrackingPriceTestCase : ChromeTestCase
@end

@implementation TrackingPriceTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Feature parameters follow a key/value format to enable or disable
  // parameters.
  std::string params =
      ":enable_price_tracking/true/enable_price_notification/true";
  std::string priceNotificationsFlag =
      std::string(commerce::kCommercePriceTracking.name) + params;
  std::string shoppingListFlag = std::string("ShoppingList");
  std::string settingsMenuItem = std::string("NotificationSettingsMenuItem");

  config.additional_args.push_back(
      "--enable-features=" + priceNotificationsFlag + "," + shoppingListFlag +
      "," + settingsMenuItem);

  // Test the updated settings page when the Tips Notifications feature is
  // enabled.
  if ([self isRunningTest:@selector
            (testTrackingPriceSwipeDown_FromUpdatedSettingsView)]) {
    config.additional_args.push_back("--enable-features=IOSTipsNotifications");
  } else {
    config.additional_args.push_back("--disable-features=IOSTipsNotifications");
  }

  return config;
}

// Tests that the settings page is dismissed by swiping down from the top.
// TODO(crbug.com/326070899): remove this test when Tips Notifications is
// enabled by default.
// TODO(crbug.com/341308196): reenable for device.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testTrackingPriceSwipeDown DISABLED_testTrackingPriceSwipeDown
#else
#define MAYBE_testTrackingPriceSwipeDown testTrackingPriceSwipeDown
#endif
- (void)MAYBE_testTrackingPriceSwipeDown {
  [self openTrackingPriceSettings];

  // Check that Tracking Price TableView is presented.
  [[EarlGrey selectElementWithMatcher:SettingsTrackingPriceTableView()]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:SettingsTrackingPriceTableView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that Settings has been dismissed.
  [[EarlGrey selectElementWithMatcher:SettingsTrackingPriceTableView()]
      assertWithMatcher:grey_nil()];
}

// Tests that the settings page is dismissed by swiping down from the top.
- (void)testTrackingPriceSwipeDown_FromUpdatedSettingsView {
  [self openTrackingPriceSettingsFromUpdatedSettingsView];

  // Check that Tracking Price TableView is presented.
  [[EarlGrey selectElementWithMatcher:SettingsTrackingPriceTableView()]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:SettingsTrackingPriceTableView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that Settings has been dismissed.
  [[EarlGrey selectElementWithMatcher:SettingsTrackingPriceTableView()]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Helpers

// Opens tracking price settings from price notifications setting page.
- (void)openTrackingPriceSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuNotificationsButton()];
  [ChromeEarlGreyUI tapPriceNotificationsMenuButton:
                        ButtonWithAccessibilityLabelId(
                            IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACKING_TITLE)];
}

// Opens tracking price settings from updated notifications setting page.
- (void)openTrackingPriceSettingsFromUpdatedSettingsView {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuNotificationsButton()];
  id priceTrackingCell =
      grey_allOf(chrome_test_util::ContainsPartialText(l10n_util::GetNSString(
                     IDS_IOS_NOTIFICATIONS_OPT_IN_PRICE_TRACKING_TOGGLE_TITLE)),
                 grey_sufficientlyVisible(), nil);
  [ChromeEarlGreyUI tapPriceNotificationsMenuButton:priceTrackingCell];
}

@end
