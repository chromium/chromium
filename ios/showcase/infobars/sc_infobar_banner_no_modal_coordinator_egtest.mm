// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/showcase/infobars/sc_infobar_constants.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::showcase_utils::Open;
using ::showcase_utils::Close;
}

// Tests for the Infobar Banner with No Modal support.
@interface SCInfobarBannerNoModalTestCase : ShowcaseTestCase
@end

@implementation SCInfobarBannerNoModalTestCase

- (void)setUp {
  [super setUp];
  Open(@"Infobar Banner No Modal");
}

- (void)tearDown {
  Close();
  [super tearDown];
}

// Tests that the InfobarBanner is correctly displaying its Labels.
- (void)testInfobarBannerConfiguration {
  // Check Banner was presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Check labels and dissmiss Banner as cleanup.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          kInfobarBannerTitleLabel)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          kInfobarBannerSubtitleLabel)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(
                                              kInfobarBannerButtonLabel),
                                          grey_accessibilityTrait(
                                              UIAccessibilityTraitButton),
                                          nil)] performAction:grey_tap()];
}

// Tests that the InfobarBanner is dismissed correctly when its accept button is
// tapped.
- (void)testInfobarBannerDismissButton {
  // Check Banner was presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Tap the Accept Button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerAcceptButtonIdentifier)]
      performAction:grey_tap()];
  // Check Banner was dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that the InfobarBanner is dismissed correctly when is swiped up.
- (void)testInfobarBannerDismissSwipe {
  // Check Banner was presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Swipe up the Banner.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  // Check Banner was dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that the InfobarModal is not presented when the Banner is swiped down.
- (void)testInfobarBannerCantSwipeDown {
  // Check Banner was presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Swipe Banner down.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  // Check the Modal is not presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerPresentedModalLabel)]
      assertWithMatcher:grey_nil()];
  // Check the banner is still interactable by swiping it up.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  // Check Banner was dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that the InfobarModal is not presented when the Banner is tapped.
- (void)testInfobarBannerTapped {
  // Check Banner was presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Tap Banner.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      performAction:grey_tap()];
  // Check the Modal is not presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerPresentedModalLabel)]
      assertWithMatcher:grey_nil()];
  // Check the banner is still interactable by swiping it up.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  // Check Banner was dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that there's no Infobar Open Modal button. Banners that don't present a
// Modal shouldn't have one.
- (void)testInfobarBannerNoGear {
  // Check Banner was presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Check Gear Button is not present.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kInfobarBannerOpenModalButtonIdentifier)]
      assertWithMatcher:grey_notVisible()];
  // Dismiss banner.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
}

@end
