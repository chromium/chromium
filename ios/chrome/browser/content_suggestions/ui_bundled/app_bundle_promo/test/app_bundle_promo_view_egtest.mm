// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/strings/stringprintf.h"
#import "base/test/ios/wait_util.h"
#import "components/segmentation_platform/embedder/home_modules/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/device_form_factor.h"

namespace {

// Accessibility ID for an `AppBundlePromoView`.
NSString* const kAppBundlePromoViewID = @"kAppBundlePromoViewID";

// Scrolls to the App Bundle promo card.
void ScrollToAppBundlePromo() {
  id<GREYMatcher> magicStackScrollView =
      grey_accessibilityID(kMagicStackScrollViewAccessibilityIdentifier);
  CGFloat moduleSwipeAmount = kMagicStackWideWidth * 0.6;
  id<GREYMatcher> appBundlePromoMatcher = grey_allOf(
      grey_accessibilityID(kAppBundlePromoViewID), grey_interactable(), nil);

  [[EarlGrey selectElementWithMatcher:appBundlePromoMatcher]
         usingSearchAction:GREYScrollInDirectionWithStartPoint(
                               kGREYDirectionRight, moduleSwipeAmount, 0.9, 0.5)
      onElementWithMatcher:magicStackScrollView];
}

// Checks App Bundle promo card disappears.
void WaitForAppBundlePromoToDisappear() {
  GREYCondition* module_disappeared = [GREYCondition
      conditionWithName:@"App Bundle Promo Disappears"
                  block:^BOOL {
                    NSError* error;
                    [[EarlGrey
                        selectElementWithMatcher:grey_accessibilityID(
                                                     kAppBundlePromoViewID)]
                        assertWithMatcher:grey_notVisible()
                                    error:&error];
                    return error == nil;
                  }];
  GREYAssertTrue([module_disappeared
                     waitWithTimeout:base::test::ios::kWaitForUIElementTimeout
                                         .InSecondsF()],
                 @"App Bundle Promo Module disappeared.");
}

}  // namespace

// Test case for the App Bundle promo view in the Magic Stack.
@interface AppBundlePromoViewTestCase : ChromeTestCase

@end

@implementation AppBundlePromoViewTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      "--test-ios-module-ranker=app_bundle_promo_ephemeral_module");
  std::string forceShowFlag = base::StringPrintf(
      "--enable-features=%s:%s/%s",
      segmentation_platform::features::kSegmentationPlatformEphemeralCardRanker
          .name,
      segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
      segmentation_platform::kAppBundlePromoEphemeralModule);

  config.features_enabled.push_back(
      segmentation_platform::features::kAppBundlePromoEphemeralCard);
  config.additional_args.push_back(forceShowFlag);

  return config;
}

// Tests that the promo card appears with the correct text.
- (void)testPromoCardAppearsCorrectly {
  ScrollToAppBundlePromo();
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_MAGIC_STACK_APP_BUNDLE_PROMO_CARD_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::StaticTextWithAccessibilityLabelId(
              (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
                  ? IDS_IOS_MAGIC_STACK_APP_BUNDLE_PROMO_CARD_IPAD_DESCRIPTION
                  : IDS_IOS_MAGIC_STACK_APP_BUNDLE_PROMO_CARD_IPHONE_DESCRIPTION)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the promo card disappears after being tapped.
- (void)testPromoCardDisappears {
  ScrollToAppBundlePromo();
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kAppBundlePromoViewID)]
      performAction:grey_tap()];
  WaitForAppBundlePromoToDisappear();
}

@end
