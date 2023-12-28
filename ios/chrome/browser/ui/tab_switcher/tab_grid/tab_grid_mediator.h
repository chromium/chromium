// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mutator.h"

@protocol GridToolbarsMutator;
@protocol TabGridConsumer;
@protocol TabGridPageMutator;

class PrefService;

// Mediates between model layer and tab grid UI layer.
@interface TabGridMediator : NSObject <TabGridMutator>

// Mutator for regular Tabs.
@property(nonatomic, weak) id<TabGridPageMutator> regularPageMutator;
// Mutator for incognito Tabs.
@property(nonatomic, weak) id<TabGridPageMutator> incognitoPageMutator;
// Mutator for remote Tabs.
@property(nonatomic, weak) id<TabGridPageMutator> remotePageMutator;

// Mutator to handle toolbars modification.
@property(nonatomic, weak) id<GridToolbarsMutator> toolbarsMutator;

// Consumer for state changes in tab grid.
@property(nonatomic, weak) id<TabGridConsumer> consumer;

- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
// Set the current displayed page (incognito, regular or remote).
- (void)setPage:(TabGridPage)page;
// Set the current mode (normal/selection/search/inactive) on the currently
// displayed page.
- (void)setModeOnCurrentPage:(TabGridMode)mode;
// Stops mediating and disconnects from backend models.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MEDIATOR_H_
