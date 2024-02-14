// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_mutator.h"

@protocol TabCollectionConsumer;
@protocol TabGroupConsumer;
class WebStateList;

// Tab group mediator in charge to handle model update for one group.
@interface TabGroupMediator : BaseGridMediator <TabGroupMutator>

// TODO(crbug.com/1501837): Add a tab group ID when the ID will be available.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                            consumer:(id<TabGroupConsumer>)consumer
                        gridConsumer:(id<TabCollectionConsumer>)gridConsumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_MEDIATOR_H_
