// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#include "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Integration tests for the Start Surface user flows.
@interface StartSurfaceTestCase : ChromeTestCase
@end

@implementation StartSurfaceTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      std::string("--enable-features=StartSurface<StartSurface"));
  config.additional_args.push_back(
      std::string("--force-fieldtrials=StartSurface/ShrinkLogo"));
  config.additional_args.push_back(
      std::string("--force-fieldtrial-params=StartSurface.ShrinkLogo:"
                  "ReturnToStartSurfaceInactiveDurationInSeconds/0"));
  return config;
}

// Tests that navigating to a page and restarting upon cold start, an NTP page
// is opened.
- (void)testColdStartOpenStartSurface {
  // TODO(crbug.com/1198227): Reenable this test when the session saving issue
  // is resolved.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (Session saving issue)");
  }

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:[self appConfigurationForTestCase]];

  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
  // Assert NTP is visible by checking that the fake omnibox is here.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2,
                  @"Two tabs were expected to be open");
}

@end
