// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_CREATE_TAB_GROUP_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_CREATE_TAB_GROUP_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_creation_consumer.h"

@protocol CreateOrEditTabGroupViewControllerDelegate;
@protocol TabGroupsCommands;
@protocol TabGroupCreationMutator;

// View controller that display the tab group creation view.
@interface CreateTabGroupViewController
    : UIViewController <TabGroupCreationConsumer>

// Delegate.
@property(nonatomic, weak) id<CreateOrEditTabGroupViewControllerDelegate>
    delegate;

// Mutator to handle model changes.
@property(nonatomic, weak) id<TabGroupCreationMutator> mutator;

// Initiates a CreateTabGroupViewController in `editMode`. If NO, then the view
// controller is in creation mode. `tabSynced` represents whether the tabs are
// synced across devices.
- (instancetype)initWithEditMode:(BOOL)editMode
                       tabSynced:(BOOL)tabSynced;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_CREATE_TAB_GROUP_VIEW_CONTROLLER_H_
