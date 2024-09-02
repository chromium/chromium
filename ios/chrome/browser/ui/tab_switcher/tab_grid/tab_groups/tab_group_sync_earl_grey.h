// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_SYNC_EARL_GREY_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_SYNC_EARL_GREY_H_

#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"

#define TabGroupSyncEarlGrey \
  [TabGroupSyncEarlGreyImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

// Methods used for the EarlGrey tests to modify FakeTabGroupSyncService.
@interface TabGroupSyncEarlGreyImpl : BaseEGTestHelperImpl

// Creates and saves 3 saved tab groups.
- (void)prepareFakeSavedTabGroups;

// Removes a group at `index`. `index` should be less than the number of groups.
- (void)removeAtIndex:(unsigned int)index;

// Removes all saved tab groups.
- (void)cleanup;

// Returns the number of saved tab groups.
- (int)countOfSavedTabGroups;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_SYNC_EARL_GREY_H_
