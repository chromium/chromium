// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_CREATION_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_CREATION_MUTATOR_H_

#import <UIKit/UIKit.h>

#import "components/tab_groups/tab_group_color.h"

// Mutator to manage the creation of a tab group.
@protocol TabGroupCreationMutator

// Tells the receiver to create a new group with the given title and color.
// Calls the completion after the group is created.
- (void)createNewGroupWithTitle:(NSString*)title
                          color:(tab_groups::TabGroupColorId)colorID
                     completion:(void (^)())completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_CREATION_MUTATOR_H_
