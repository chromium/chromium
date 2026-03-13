// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/omnibox/browser/omnibox_pref_names.h"
#import "ios/chrome/browser/autocomplete/test/autocomplete_app_interface.h"
#import "ios/chrome/browser/composebox/eg_tests/inttest/composebox_inttest_app_interface.h"
#import "ios/chrome/browser/composebox/eg_tests/inttest/composebox_inttest_earl_grey.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/omnibox/eg_tests/omnibox_matchers.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_coordinator_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

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
  config.features_enabled.push_back(kComposeboxIpad);

  if ([self isRunningTest:@selector(testSearchWithAIMDisabled)]) {
    config.features_enabled.push_back(kComposeboxAIMDisabled);
  }

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

#pragma mark - Tests

// Tests that a search can be performed from Composebox when AIM is disabled.
// This validates the fallback path where ContextualSearchSession is null.
- (void)testSearchWithAIMDisabled {
  NSString* search_query = @"test search with aim disabled";
  [ChromeEarlGreyUI replaceTextInOmnibox:search_query];

  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  [ComposeboxInttestEarlGrey assertSearchLoaded:search_query];
}

@end
