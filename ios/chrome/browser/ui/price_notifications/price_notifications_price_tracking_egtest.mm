// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/commerce/core/commerce_feature_list.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface PriceNotificationsPriceTrackingTestCase : ChromeTestCase
@end

@implementation PriceNotificationsPriceTrackingTestCase

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

- (void)testPriceTrackingDismissButton {
  // TODO(crbug.com/1478755): Investigate why this test fails with
  // ReplaceSyncWithSignin.
  if ([ChromeEarlGrey isReplaceSyncWithSigninEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"crbug.com/1478755: Temporarily disabled.");
  }

  [self signinPriceTrackingUser];
  [self openTrackingPriceUI];

  // Close the Price Tracking UI via the Dismiss button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewNavigationDismissButtonId)]
      assertWithMatcher:grey_notNil()];
}

// Confirms the Price Tracking carousel destination is not visible when the user
// is in Incognito.
- (void)testPriceTrackingIsNotVisibleInIncognito {
  CGFloat const kMenuScrollDisplacement = 150;
  id<GREYAction> scrollRight =
      grey_scrollInDirection(kGREYDirectionRight, kMenuScrollDisplacement);
  id<GREYAction> scrollDown =
      grey_scrollInDirection(kGREYDirectionDown, kMenuScrollDisplacement);
  id<GREYMatcher> interactableSettingsButton =
      grey_allOf(chrome_test_util::PriceNotificationsDestinationButton(),
                 grey_interactable(), nil);
  id<GREYAction> scrollAction =
      [ChromeEarlGrey isNewOverflowMenuEnabled] ? scrollRight : scrollDown;

  [self signinPriceTrackingUser];
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey selectElementWithMatcher:interactableSettingsButton]
         usingSearchAction:scrollAction
      onElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Helpers

// Opens price tracking UI from the overflow menu carousel.
- (void)openTrackingPriceUI {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::
                             PriceNotificationsDestinationButton()];
}

// Signin into Chrome in order for the Price Tracking Destination to appear.
- (void)signinPriceTrackingUser {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
}

@end
