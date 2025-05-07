// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_CREATE_TAB_GROUP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_CREATE_TAB_GROUP_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <set>

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_creation_mutator.h"

class Browser;
@protocol CreateTabGroupMediatorDelegate;
class FaviconLoader;
class TabGroup;
@protocol TabGroupCreationConsumer;
class WebStateList;

namespace web {
class WebStateID;
}

// Mediator to manage the model layer of the tab group creation.
@interface CreateTabGroupMediator : NSObject <TabGroupCreationMutator>

// The delegate gets notified of lifecycle events.
@property(nonatomic, weak) id<CreateTabGroupMediatorDelegate> delegate;

// Init the tab group creation mediator with:
// - `consumer` the UI that will receive updates.
// - `identifiers` the list of selected tabs ID
// - `browser` the browser containing the selected tabs.
// `faviconLoader`: used to fetch favicons on Google server, can be `nullptr`.
- (instancetype)
    initTabGroupCreationWithConsumer:(id<TabGroupCreationConsumer>)consumer
                        selectedTabs:(std::set<web::WebStateID>&)identifiers
                             browser:(Browser*)browser
                       faviconLoader:(FaviconLoader*)faviconLoader;

// Init the tab group creation mediator with:
// - `consumer` the UI that will receive updates.
// - `tabGroup` the group to edit.
// - `webStateList` the web state list containing `tabGroup`.
// `faviconLoader`: used to fetch favicons on Google server, can be `nullptr`.
- (instancetype)initTabGroupEditionWithConsumer:
                    (id<TabGroupCreationConsumer>)consumer
                                       tabGroup:(const TabGroup*)tabGroup
                                   webStateList:(WebStateList*)webStateList
                                  faviconLoader:(FaviconLoader*)faviconLoader;

// Disconnects the mediator's dependencies.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_CREATE_TAB_GROUP_MEDIATOR_H_
