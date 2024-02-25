// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Identifer for group cell at given `index` in the tab grid.
NSString* IdentifierForGroupCellAtIndex(unsigned int index) {
  return [NSString
      stringWithFormat:@"%@%u", kGroupGridCellIdentifierPrefix, index];
}
}  // namespace

// Test Tab Groups feature.
@interface TabGroupsTestCase : ChromeTestCase
@end

@implementation TabGroupsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabGroupsInGrid);
  return config;
}

// Tests if the tab group creation view is displayed after pushing the button in
// the context menu.
- (void)testCreateTabGroupIsDisplayedAfterLongPressATab {
  // Ensure the app is clean.
  // TODO(crbug.com/1501837): Remove this workaround when the feature is
  // finished. When run repeatedly, the view is never dismissed, which cause the
  // test to fail.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [ChromeEarlGreyUI openTabGrid];

  // TODO(crbug.com/1501837): Use chrome_test_util::TabGridCellAtIndex(0) when
  // cells are not group cells anymore.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              IdentifierForGroupCellAtIndex(0)),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_longPress()];

  // TODO(crbug.com/1501837): Remove this matcher and replace it with "create
  // new group" option.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_CONTENT_CONTEXT_RENAMEGROUP))]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kCreateTabGroupIdentifier)]
      assertWithMatcher:grey_notNil()];
}

@end
