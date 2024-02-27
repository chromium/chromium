// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_CREATE_TAB_GROUP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_CREATE_TAB_GROUP_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <set>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_creation_mutator.h"

class TabGroup;
@protocol TabGroupCreationConsumer;
class WebStateList;

namespace web {
class WebStateID;
}

// Mediator to manage the model layer of the tab group creation.
@interface CreateTabGroupMediator : NSObject <TabGroupCreationMutator>

// Init the tab group creation mediator with:
// - `consumer` the UI that will receive updates.
// - `identifiers` the list of selected tabs ID
// - `webStateList` the web state list where the `identifiers` are from
- (instancetype)
    initTabGroupCreationWithConsumer:(id<TabGroupCreationConsumer>)consumer
                        selectedTabs:(std::set<web::WebStateID>&)identifiers
                        webStateList:(WebStateList*)webStateList;

// Init the tab group creation mediator with:
// - `consumer` the UI that will receive updates.
// - `tabGroup` the group to edit
- (instancetype)initTabGroupEditionWithConsumer:
                    (id<TabGroupCreationConsumer>)consumer
                                       tabGroup:(const TabGroup*)tabGroup
                                   webStateList:(WebStateList*)webStateList;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_CREATE_TAB_GROUP_MEDIATOR_H_
