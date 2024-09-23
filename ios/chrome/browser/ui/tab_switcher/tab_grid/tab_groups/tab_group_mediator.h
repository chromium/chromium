// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_mutator.h"

class TabGroup;
@protocol TabCollectionConsumer;
@class TabGridModeHolder;
@protocol TabGroupsCommands;
@protocol TabGroupConsumer;
class WebStateList;

// Tab group mediator in charge to handle model update for one group.
@interface TabGroupMediator : BaseGridMediator <TabGroupMutator>

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                            tabGroup:(base::WeakPtr<const TabGroup>)tabGroup
                            consumer:(id<TabGroupConsumer>)consumer
                        gridConsumer:(id<TabCollectionConsumer>)gridConsumer
                          modeHolder:(TabGridModeHolder*)modeHolder;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_MEDIATOR_H_
