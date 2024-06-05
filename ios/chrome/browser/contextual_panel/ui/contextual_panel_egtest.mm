// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  // Enable User Policy for both consent levels.
  config.features_enabled.push_back(kContextualPanel);
  config.features_enabled.push_back(kContextualPanelForceShowEntrypoint);
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

@end
