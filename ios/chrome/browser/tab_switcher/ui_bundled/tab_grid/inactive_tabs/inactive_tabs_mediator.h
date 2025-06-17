// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_toolbars_configuration_provider.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_view_controller_mutator.h"

@protocol InactiveTabsInfoConsumer;
@class InactiveTabsMediator;
class PrefService;
class TabsCloser;
@protocol SnapshotStorage;
@protocol TabCollectionConsumer;
class WebStateList;

// Delegate for the InactiveTabsMediator.
@protocol InactiveTabsMediatorDelegate

// Tells the delegate that there are no longer any inactive tabs.
- (void)inactiveTabsMediatorEmpty:(InactiveTabsMediator*)inactiveTabsMediator;

@end

// This mediator provides data to the Inactive Tabs grid and handles
// interactions.
@interface InactiveTabsMediator : NSObject <GridCommands,
                                            GridToolbarsConfigurationProvider,
                                            GridViewControllerMutator>

// `consumer` receives `webStateList` and Inactive Tabs info updates.
@property(nonatomic, weak) id<TabCollectionConsumer, InactiveTabsInfoConsumer>
    consumer;

// Delegate for the mediator.
@property(nonatomic, weak) id<InactiveTabsMediatorDelegate> delegate;

// Initializer with:
// - `webStateList`: the list of tabs to observe.
// - `prefService`: the preference service from the profile.
// - `snapshotStorage`: the snapshot storage from the inactive browser.
// - `tabsCloser`: the object used to implement "close all" and "undo".
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                  profilePrefService:(PrefService*)prefService
                     snapshotStorage:(id<SnapshotStorage>)snapshotStorage
                          tabsCloser:(std::unique_ptr<TabsCloser>)tabsCloser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Returns the number of items pushed to the consumer.
- (NSInteger)numberOfItems;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_MEDIATOR_H_
