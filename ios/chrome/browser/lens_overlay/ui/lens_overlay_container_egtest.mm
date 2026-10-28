// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/test/lens_overlay_app_interface.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_accessibility_identifier_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "url/gurl.h"

@interface LensOverlayContainerTestCase : ChromeTestCase
@end

@implementation LensOverlayContainerTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self isRunningTest:@selector(testOpenLensViewFinderInBackground)]) {
    config.additional_args.push_back(
        std::string("--") +
        std::string(test_switches::kEnableFakeLensViewFinder));
  }
  return config;
}

// Tests that opening the Lens View Finder while the app is in the background
// does not crash (regression test for crbug.com/573839026).
- (void)testOpenLensViewFinderInBackground {
  // Load a web page so BrowserViewController's steady toolbar and location bar
  // layout guides are attached before snapshotting during the transition.
  [ChromeEarlGrey loadURL:GURL("about:blank")];

  // Move the application to the background.
  GREYAssertTrue([[AppLaunchManager sharedManager] backgroundApplication],
                 @"Failed to background application.");

  // Present the Lens View Finder while the scene is in the background.
  GREYAssertTrue(
      [LensOverlayAppInterface presentLensViewFinderInWindowWithNumber:0],
      @"Failed to present Lens View Finder in background.");

  // Return to the foreground and verify the app is still alive.
  [[[XCUIApplication alloc] init] activate];
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
