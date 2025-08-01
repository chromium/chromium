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

// Creates and saves `numberOfGroups` shared tab groups with a tab using `url`.
// The user (using foo1 account) will be set as `owner` or not of the group.
+ (void)prepareFakeSharedTabGroups:(NSInteger)numberOfGroups
                           asOwner:(BOOL)owner
                               url:(NSString*)url;

// Removes a group at `index`.
+ (void)removeAtIndex:(unsigned int)index;

// Removes all saved tab groups.
+ (void)cleanup;

// Returns the number of saved tab groups.
+ (int)countOfSavedTabGroups;

// Sets the mock response for getting the shared entities preview of a group.
+ (void)mockSharedEntitiesPreview;

// Adds a tab to the group specified by `index`. `index` considers only groups
// not tabs. The group should be shared already and the tab is added by a member
// (fakeIdentity3).
+ (void)addSharedTabToGroupAtIndex:(unsigned int)index;

// Returns the URL of the activity logs.
+ (NSString*)activityLogsURL;

// Updates the Shared tab groups' managed account policy status.
+ (void)setSharedTabGroupsManagedAccountPolicyEnabled:
    (BOOL)managedAccountPolicyEnabled;

// Whether the Shared Tab Groups feature is enabled and a user can join to an
// existing shared group.
+ (BOOL)isAllowedToJoinTabGroups;

// Whether the Shared Tab Groups feature is enabled and a user can create a new
// shared group.
+ (BOOL)isAllowedToShareTabGroups;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_APP_INTERFACE_H_
