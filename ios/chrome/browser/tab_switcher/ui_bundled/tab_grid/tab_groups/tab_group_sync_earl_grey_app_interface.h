// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_SYNC_EARL_GREY_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_SYNC_EARL_GREY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// App-side implementation for TabGroupSyncEarlGrey.
@interface TabGroupSyncEarlGreyAppInterface : NSObject

// Creates and saves `numberOfGroups` saved tab groups.
+ (void)prepareFakeSavedTabGroups:(NSInteger)numberOfGroups;

// Removes a group at `index`.
+ (void)removeAtIndex:(unsigned int)index;

// Removes all saved tab groups.
+ (void)cleanup;

// Returns the number of saved tab groups.
+ (int)countOfSavedTabGroups;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_SYNC_EARL_GREY_APP_INTERFACE_H_
