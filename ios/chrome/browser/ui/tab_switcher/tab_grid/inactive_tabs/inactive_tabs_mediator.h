// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_configuration_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_controller_mutator.h"

@protocol InactiveTabsInfoConsumer;
class PrefService;
class TabsCloser;
@class SnapshotStorageWrapper;
@protocol TabCollectionConsumer;
class WebStateList;

// This mediator provides data to the Inactive Tabs grid and handles
// interactions.
@interface InactiveTabsMediator : NSObject <GridCommands,
                                            GridToolbarsConfigurationProvider,
                                            GridViewControllerMutator>

// `consumer` receives `webStateList` and Inactive Tabs info updates.
@property(nonatomic, weak) id<TabCollectionConsumer, InactiveTabsInfoConsumer>
    consumer;

// Initializer with:
// - `webStateList`: the list of tabs to observe.
// - `prefService`: the preference service from the application context.
// - `snapshotStorage`: the snapshot storage from the inactive browser.
// - `tabsCloser`: the object used to implement "close all" and "undo".
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                         prefService:(PrefService*)prefService
                     snapshotStorage:(SnapshotStorageWrapper*)snapshotStorage
                          tabsCloser:(std::unique_ptr<TabsCloser>)tabsCloser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Returns the number of items pushed to the consumer.
- (NSInteger)numberOfItems;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_MEDIATOR_H_
