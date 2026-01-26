// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

@interface ToolbarTestCase : ChromeTestCase
@end

@implementation ToolbarTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kChromeNextIa);
  return config;
}

// Tests loading a page and checking that the URL is displayed in the location
// bar.
- (void)testLoadPage {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL("/echo");

  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::DefocusedLocationView(),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:chrome_test_util::LocationViewContainingText(
                            self.testServer->base_url().GetHost())];
}

@end
