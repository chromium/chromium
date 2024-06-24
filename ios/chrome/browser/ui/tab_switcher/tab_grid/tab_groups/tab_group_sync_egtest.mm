// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

// Returns the matcher for the Tab Groups view as third panel of Tab Grid.
id<GREYMatcher> TabGroupsPanel() {
  return grey_allOf(grey_accessibilityID(kTabGroupsPanelIdentifier),
                    grey_sufficientlyVisible(), nil);
}

}  // namespace

// Test Tab Group Sync feature.
@interface TabGroupSyncTestCase : ChromeTestCase
@end

@implementation TabGroupSyncTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabGroupsInGrid);
  config.features_enabled.push_back(kTabGroupsIPad);
  config.features_enabled.push_back(kModernTabStrip);
  config.features_enabled.push_back(kTabGroupSync);
  return config;
}

// Tests that the third panel is Tab Groups.
- (void)testThirdPanelIsTabGroups {
  [ChromeEarlGreyUI openTabGrid];

  // Switch over to Tab Groups.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGroupsPanel()]
      assertWithMatcher:grey_notNil()];
}

@end
