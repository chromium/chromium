// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_constants.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

// Tests for the tab group indicator.
@interface TanGroupIndicatorTestCase : ChromeTestCase
@end

@implementation TanGroupIndicatorTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabGroupsInGrid);
  config.features_enabled.push_back(kTabGroupsIPad);
  config.features_enabled.push_back(kModernTabStrip);
  config.features_enabled.push_back(kTabGroupSync);
  config.features_enabled.push_back(kTabGroupIndicator);
  return config;
}

// Tests that the tab group indicator view exists.
- (void)testTabGroupIndicator {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTabGroupIndicatorViewIdentifier)]
      assertWithMatcher:grey_notNil()];
}

@end
