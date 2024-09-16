// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>

#import "base/strings/stringprintf.h"
#import "base/test/ios/wait_util.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/segmentation_platform/public/features.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_constants.h"
#import "ios/chrome/browser/ui/push_notification/scoped_notification_auth_swizzler.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

// Checks Price Tracking Promo module disappears.
void WaitForPriceTrackingPromoToDisappear() {
  GREYCondition* module_shown = [GREYCondition
      conditionWithName:@"Price Tracking Promo Module Disappears"
                  block:^BOOL {
                    NSError* error;
                    [[EarlGrey
                        selectElementWithMatcher:grey_accessibilityID(
                                                     kPriceTrackingPromoViewID)]
                        assertWithMatcher:grey_notVisible()
                                    error:&error];
                    return error == nil;
                  }];

  GREYAssertTrue(
      [module_shown waitWithTimeout:base::test::ios::kWaitForUIElementTimeout
                                        .InSecondsF()],
      @"Price Tracking Promo Module was visible.");
}

}  // namespace

// Test case for the Price Tracking Promo view in the Magic Stack module.
@interface PriceTrackingPromoViewTestCase : ChromeTestCase
@end

@implementation PriceTrackingPromoViewTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      "--test-ios-module-ranker=price_tracking_promo");
  std::string enableFeatures = base::StringPrintf(
      "--enable-features=%s,%s:%s/%s", commerce::kPriceTrackingPromo.name,
      segmentation_platform::features::kSegmentationPlatformEphemeralCardRanker
          .name,
      segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
      segmentation_platform::features::kPriceTrackingPromoForceOverride);
  config.additional_args.push_back(enableFeatures);
  return config;
}

// Tests the first opt in flow of the price tracking promo card in the magic
// stack whereby the user has not opted into notifications in the app at all.
// TODO(crbug.com/367287646): Test fails on iphone-device.
- (void)DISABLED_testFirstOptInFlow {
  ScopedNotificationAuthSwizzler auth(YES);
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_ALLOW)]
      performAction:grey_tap()];
  WaitForPriceTrackingPromoToDisappear();
}

@end
