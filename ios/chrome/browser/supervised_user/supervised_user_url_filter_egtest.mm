// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/feature_list.h"
#import "components/supervised_user/core/common/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/net_errors.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests the SupervisedUserURLFilter.
@interface SupervisedUserURLFilterTestCase : ChromeTestCase
@end

@implementation SupervisedUserURLFilterTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
  return config;
}

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (void)tearDown {
  [super tearDown];
}

// Tests that a page load is blocked when the URL is filtered.
- (void)testBlockedSiteDisplaysErrorOnPageLoad {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/filtered")];
  [ChromeEarlGrey
      waitForWebStateContainingText:net::ErrorToShortString(
                                        net::ERR_BLOCKED_BY_ADMINISTRATOR)];
}

// Tests that unfiltered sites are loaded normally.
- (void)testUnfilteredSiteIsLoaded {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  // Expected text set by embedded_test_server handler.
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
}

@end
