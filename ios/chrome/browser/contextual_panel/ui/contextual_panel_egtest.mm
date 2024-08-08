// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

@interface ContextualPanelTestCase : ChromeTestCase
@end

@implementation ContextualPanelTestCase

- (void)setUp {
  [super setUp];
  bool started = self.testServer->Start();
  GREYAssertTrue(started, @"Test server failed to start.");
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  if ([self isRunningTest:@selector(testOpenContextualPanelFromNormalIPH)]) {
    config.features_enabled_and_params.push_back(
        {kContextualPanel, {{{"entrypoint-rich-iph", "false"}}}});
  } else {
    config.features_enabled_and_params.push_back({kContextualPanel, {}});
  }

  config.features_enabled_and_params.push_back(
      {kContextualPanelForceShowEntrypoint, {}});

  config.iph_feature_enabled =
      feature_engagement::kIPHiOSContextualPanelSampleModelFeature.name;

  return config;
}

// Tests that the contextual panel opens correctly.
- (void)testOpenContextualPanel {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/defaultresponse")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"ContextualPanelEntrypointImageViewAXID")]
      performAction:grey_tap()];

  // Check that the contextual panel opened up.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelContentViewAXID")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close panel
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelCloseButtonAXID")]
      performAction:grey_tap()];
}

// Tests that the contextual panel opens correctly when tapping the rich IPH.
- (void)testOpenContextualPanelFromRichIPH {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/defaultresponse")];

  // Check that the IPH has appeared.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              @"BubbleViewLabelIdentifier")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"BubbleViewLabelIdentifier")]
      performAction:grey_tap()];

  // Check that the contextual panel opened up.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelContentViewAXID")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close panel
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelCloseButtonAXID")]
      performAction:grey_tap()];
}

// Tests that the contextual panel opens correctly when tapping the normal IPH.
- (void)testOpenContextualPanelFromNormalIPH {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/defaultresponse")];

  // Check that the IPH has appeared.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              @"BubbleViewLabelIdentifier")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"BubbleViewLabelIdentifier")]
      performAction:grey_tap()];

  // Check that the contextual panel opened up.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelContentViewAXID")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close panel
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelCloseButtonAXID")]
      performAction:grey_tap()];
}

// Test that the Contextual Panel can still be closed after rotating to
// landscape.
- (void)testContextualPanelLandscape {
  // This test is not relevant on iPads as iPads aren't compact height in
  // landscape.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/defaultresponse")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"ContextualPanelEntrypointImageViewAXID")]
      performAction:grey_tap()];

  // Check that the contextual panel opened up.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelContentViewAXID")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Switch to landscape.
  GREYAssert(
      [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                    error:nil],
      @"Could not rotate device to Landscape Left");

  // Make sure that panel can still be closed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelCloseButtonAXID")]
      performAction:grey_tap()];
}

// Tests that closing the last tab with the panel open doesn't crash.
- (void)testCloseLastTabWithPanelOpen {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/defaultresponse")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"ContextualPanelEntrypointImageViewAXID")]
      performAction:grey_tap()];

  // Check that the contextual panel opened up.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelContentViewAXID")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close the tab.
  [ChromeEarlGrey closeTabAtIndex:0];
}

@end
