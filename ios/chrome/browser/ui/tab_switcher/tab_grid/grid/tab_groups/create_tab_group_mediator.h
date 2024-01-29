// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_CREATE_TAB_GROUP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_CREATE_TAB_GROUP_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_creation_mutator.h"

@protocol TabGroupCreationConsumer;

// Mediator to manage the model layer of the tab group creation.
@interface CreateTabGroupMediator : NSObject <TabGroupCreationMutator>

- (instancetype)initWithConsumer:(id<TabGroupCreationConsumer>)consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_CREATE_TAB_GROUP_MEDIATOR_H_
