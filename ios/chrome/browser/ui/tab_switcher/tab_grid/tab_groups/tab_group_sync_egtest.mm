// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_sync_earl_grey.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::TabGridTabGroupsPanelButton;

namespace {

NSString* const kSavedGroup1Name = @"1RemoteGroup";
NSString* const kSavedGroup2Name = @"2RemoteGroup";
NSString* const kSavedGroup3Name = @"3RemoteGroup";

// Returns the matcher for the Tab Groups view as third panel of Tab Grid.
id<GREYMatcher> TabGroupsPanel() {
  return grey_allOf(grey_accessibilityID(kTabGroupsPanelIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for tab groups panel cell for the given `group_name` and `tab_count`.
// Note that it only matches with a group created just now.
id<GREYMatcher> TabGroupsPanelCellMatcher(NSString* group_name,
                                          NSInteger tab_count) {
  NSString* number_of_tabs_string =
      l10n_util::GetPluralNSStringF(IDS_IOS_TAB_GROUP_TABS_NUMBER, tab_count);
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSStringF(
          IDS_IOS_TAB_GROUPS_PANEL_CELL_ACCESSIBILITY_LABEL_FORMAT,
          base::SysNSStringToUTF16(group_name),
          base::SysNSStringToUTF16(number_of_tabs_string),
          base::SysNSStringToUTF16(@"Created just now"))),
      grey_kindOfClassName(@"TabGroupsPanelCell"), grey_sufficientlyVisible(),
      nil);
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

  // Add the flag to use FakeTabGroupSyncService.
  config.additional_args.push_back(
      "--" + std::string(test_switches::kEnableFakeTabGroupSyncService));
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

// Tests that TabGroupSyncEarlGrey creates saved tab groups correctly.
- (void)testPreparedSavedTabGroups {
  GREYAssertEqual(0, [TabGroupSyncEarlGrey countOfSavedTabGroups],
                  @"The number of saved tab groups should be 0.");
  [TabGroupSyncEarlGrey prepareFakeSavedTabGroups];
  GREYAssertEqual(3, [TabGroupSyncEarlGrey countOfSavedTabGroups],
                  @"The number of saved tab groups should be 3.");

  // Open the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Switch over to Tab Groups.
  [[EarlGrey selectElementWithMatcher:TabGridTabGroupsPanelButton()]
      performAction:grey_tap()];

  // Check that the groups exist.
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellMatcher(kSavedGroup1Name, 1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellMatcher(kSavedGroup2Name, 1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TabGroupsPanelCellMatcher(kSavedGroup3Name, 1)]
      assertWithMatcher:grey_notNil()];

  [TabGroupSyncEarlGrey cleanup];
  GREYAssertEqual(0, [TabGroupSyncEarlGrey countOfSavedTabGroups],
                  @"The number of saved tab groups should be 0.");
}

@end
