// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_CREATE_TAB_GROUP_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_CREATE_TAB_GROUP_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol TabGroupCreationMutator;
@protocol TabGroupsCommands;

// View controller that display the tab group creation view.
@interface CreateTabGroupViewController : UIViewController

// Initiates a CreateTabGroupViewController with `handler` to handle user
// action and `mutator` to handle all model changes.
- (instancetype)initWithHandler:(id<TabGroupsCommands>)handler
                        mutator:(id<TabGroupCreationMutator>)mutator;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_CREATE_TAB_GROUP_VIEW_CONTROLLER_H_
