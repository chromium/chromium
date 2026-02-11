// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/eg_tests/inttest/composebox_inttest_app_interface.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_coordinator_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"

// Tests the composebox integration with ComposeboxInttestCoordinator.
@interface ComposeboxInttestTestCase : ChromeTestCase
@end

@implementation ComposeboxInttestTestCase

- (BOOL)loadMinimalAppUI {
  return YES;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kComposeboxIOS);
  return config;
}

- (void)setUp {
  [super setUp];
  [ChromeCoordinatorAppInterface startComposeboxCoordinator];
}

- (void)tearDownHelper {
  [super tearDownHelper];
  [ChromeCoordinatorAppInterface reset];
}

- (void)testLaunchComposebox {
  // Basic test to verify the test case set up works.
}

@end
