// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mutator.h"

@protocol TabGridConsumer;
@protocol TabGridPageMutator;

class PrefService;

// Delegate allowing the tab grid coordinator to update the incognito tab grid.
// TODO(crbug.com/1457146): To remove when incognito is fully isolated.
@protocol TabGridMediatorDelegate
// Repopulates the incognito tab grid with incognito tabs if applicable.
- (void)updateIncognitoTabGridState;
@end

// Mediates between model layer and tab grid UI layer.
@interface TabGridMediator : NSObject <TabGridMutator>

// Mutator for regular Tabs.
@property(nonatomic, weak) id<TabGridPageMutator> regularPageMutator;
// Mutator for incognito Tabs.
@property(nonatomic, weak) id<TabGridPageMutator> incognitoPageMutator;
// Mutator for remote Tabs.
@property(nonatomic, weak) id<TabGridPageMutator> remotePageMutator;

// Consumer for state changes in tab grid.
@property(nonatomic, weak) id<TabGridConsumer> consumer;
// Delegate allowing the mediator to update the tab grid coordinator.
@property(nonatomic, weak) id<TabGridMediatorDelegate> delegate;

- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
// Set the current displayed page (incognito, regular or remote).
- (void)setPage:(TabGridPage)page;
// Stops mediating and disconnects from backend models.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MEDIATOR_H_
