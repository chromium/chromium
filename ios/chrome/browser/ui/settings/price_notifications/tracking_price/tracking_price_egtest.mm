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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuPriceNotificationsButton;
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

  config.additional_args.push_back(
      "--enable-features=" + priceNotificationsFlag + "," + shoppingListFlag);

  return config;
}

// Tests that the settings page is dismissed by swiping down from the top.
- (void)testTrackingPriceSwipeDown {
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

#pragma mark - Helpers

// Opens tracking price settings from price notifications setting page.
- (void)openTrackingPriceSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:SettingsMenuPriceNotificationsButton()];
  [ChromeEarlGreyUI tapPriceNotificationsMenuButton:
                        ButtonWithAccessibilityLabelId(
                            IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACKING_TITLE)];
}

@end
