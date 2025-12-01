// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_coordinator_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

// Test case for Tips Promos.
@interface TipsPromosTestCase : ChromeTestCase
@end

@implementation TipsPromosTestCase

#pragma mark - Helpers

// Taps the primary action button on a PromoStyleViewController.
- (void)tapPrimaryActionButton {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackPrimaryButton()]
      performAction:grey_tap()];
}

// Taps the secondary action button on a PromoStyleViewController.
- (void)tapSecondaryActionButton {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackSecondaryButton()]
      performAction:grey_tap()];
}

// Taps the secondary action button on a ConfirmationAlertViewController.
- (void)tapConfirmationAlertSecondaryButton {
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::ButtonStackSecondaryButton(),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
}

#pragma mark - Tests

// Tests the Lens Promo, the "Show Me How" button and view, and the "Done"
// button.
- (void)testLensPromo {
  id<GREYMatcher> promo = grey_accessibilityID(@"kLensPromoAXID");
  // Start the LensPromoCoordinator.
  [ChromeCoordinatorAppInterface startLensPromoCoordinator];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:promo];

  // Tap "Show Me How".
  [self tapSecondaryActionButton];
  id<GREYMatcher> instructions =
      grey_accessibilityID(@"kLensPromoInstructionsAXID");
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:instructions];
  // Swipe down to dismiss the instructions.
  [[EarlGrey selectElementWithMatcher:instructions]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:instructions];

  // Tap "Done".
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  GREYAssert(
      [ChromeCoordinatorAppInterface selectorWasDispatched:@"dismissLensPromo"],
      @"dismissLensPromo wasn't called");

  [ChromeCoordinatorAppInterface stopCoordinator];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:promo];
  GREYAssert([ChromeCoordinatorAppInterface
                 selectorWasDispatched:@"presentLensIconBubble"],
             @"presentLensIconBubble wasn't called");
  [ChromeCoordinatorAppInterface reset];
}

// Tests the Lens Promo's "Go To Lens" button.
- (void)testLensPromoGoToLens {
  id<GREYMatcher> promo = grey_accessibilityID(@"kLensPromoAXID");
  // Start the LensPromoCoordinator.
  [ChromeCoordinatorAppInterface startLensPromoCoordinator];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:promo];

  // Tap "Go To Lens".
  [self tapPrimaryActionButton];
  GREYAssert(
      [ChromeCoordinatorAppInterface selectorWasDispatched:@"dismissLensPromo"],
      @"dismissLensPromo wasn't called");

  [ChromeCoordinatorAppInterface stopCoordinator];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:promo];
  GREYAssert([ChromeCoordinatorAppInterface
                 selectorWasDispatched:@"openLensInputSelection:"],
             @"openLensInputSelection wasn't called");
  [ChromeCoordinatorAppInterface reset];
}

// Tests the Enhanced Safe Browsing promo.
- (void)testESBPromo {
  id<GREYMatcher> promo =
      grey_accessibilityID(@"kEnhancedSafeBrowsingPromoAXID");
  // Start the EnhancedSafeBrowsingPromoCoordinator.
  [ChromeCoordinatorAppInterface startEnhancedSafeBrowsingPromoCoordinator];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:promo];

  // Tap "Show Me How".
  [self tapSecondaryActionButton];
  id<GREYMatcher> instructions =
      grey_accessibilityID(@"kEnhancedSafeBrowsingPromoInstructionsAXID");
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:instructions];
  // Swipe down to dismiss the instructions.
  [[EarlGrey selectElementWithMatcher:instructions]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:instructions];

  [self tapPrimaryActionButton];
  GREYAssert([ChromeCoordinatorAppInterface
                 selectorWasDispatched:@"dismissEnhancedSafeBrowsingPromo"],
             @"dismissEnhancedSafeBrowsingPromo wasn't called");

  [ChromeCoordinatorAppInterface stopCoordinator];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:promo];
  GREYAssert([ChromeCoordinatorAppInterface
                 selectorWasDispatched:@"showSafeBrowsingSettings"],
             @"showSafeBrowsingSettings wasn't called");
  [ChromeCoordinatorAppInterface reset];
}

// Tests the Search What You See promo.
- (void)testSearchWhatYouSeePromo {
  id<GREYMatcher> promo = grey_accessibilityID(@"kSearchWhatYouSeePromoAXID");
  // Start the SearchWhatYouSeePromoCoordinator.
  [ChromeCoordinatorAppInterface startSearchWhatYouSeePromoCoordinator];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:promo];

  // Tap "Show Me How".
  [self tapConfirmationAlertSecondaryButton];
  id<GREYMatcher> instructions =
      grey_accessibilityID(@"kSearchWhatYouSeePromoInstructionsAXID");
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:instructions];

  // Tap "Learn More"
  [self tapConfirmationAlertSecondaryButton];
  GREYAssert(
      [ChromeCoordinatorAppInterface selectorWasDispatched:@"openURLInNewTab:"],
      @"openURLInNewTab wasn't called");

  // Swipe down to dismiss the instructions.
  [[EarlGrey selectElementWithMatcher:instructions]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:instructions];

  // Tap Close Button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCloseButton()]
      performAction:grey_tap()];
  GREYAssert([ChromeCoordinatorAppInterface
                 selectorWasDispatched:@"dismissSearchWhatYouSeePromo"],
             @"dismissSearchWhatYouSeePromo wasn't called");

  [ChromeCoordinatorAppInterface stopCoordinator];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:promo];
  [ChromeCoordinatorAppInterface reset];
}

@end
