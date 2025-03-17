// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// An app interface for updating tab groups. The implementation of helper
// methods is compiled into the app binary and they can be called from either
// the app or the test code.
@interface TabGroupAppInterface : NSObject

// Creates and saves `numberOfGroups` synced tab groups.
+ (void)prepareFakeSyncedTabGroups:(NSInteger)numberOfGroups;

// Creates and saves `numberOfGroups` shared tab groups. A user with
// `fakeIdentity1` joins the group as a member and a user with `fakeIdentity2`
// joins the group as an owner.
+ (void)prepareFakeSharedTabGroups:(NSInteger)numberOfGroups;

// Removes a group at `index`.
+ (void)removeAtIndex:(unsigned int)index;

// Removes all saved tab groups.
+ (void)cleanup;

// Returns the number of saved tab groups.
+ (int)countOfSavedTabGroups;

// Sets the mock response for getting the shared entities preview of a group.
+ (void)mockSharedEntitiesPreview;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_APP_INTERFACE_H_
