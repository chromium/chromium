// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_accessibility_identifier_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface LensOverlayContainerTestCase : ChromeTestCase
@end

@implementation LensOverlayContainerTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_disabled = {};
  config.features_enabled.push_back(kEnableLensOverlay);
  return config;
}

- (void)testShowAndHideLensOverlayContainer {
  // TODO(crbug.com/359195500): Rewrite `testShowAndHideLensOverlayContainer`
  // after accesibility label is added to the button in Lens integration
}

// Tests that when pressing the escape keyboard button, closes the overlay
// container.
- (void)testPressEscapeHidesLensOverlayContainer {
  EARL_GREY_TEST_DISABLED(@"crbug.com/359498644");
  [ChromeEarlGrey loadURL:GURL("about:blank")];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::TabShareButton()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
      performAction:grey_tap()];

  id<GREYMatcher> lensOverlayContainerMatcher =
      grey_accessibilityID(kLenscontainerViewAccessibilityIdentifier);

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:lensOverlayContainerMatcher];

  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"escape" flags:0];

  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:lensOverlayContainerMatcher];
}

@end
