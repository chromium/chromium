// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GRID_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GRID_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_mutator.h"

@protocol GridToolbarsMutator;
@protocol TabGridConsumer;
@class TabGridModeHolder;
@protocol TabGridPageMutator;

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace signin {
class IdentityManager;
}  // namespace signin

class PrefService;

// Mediates between model layer and tab grid UI layer.
@interface TabGridMediator : NSObject <TabGridMutator>

// Mutator for regular Tabs.
@property(nonatomic, weak) id<TabGridPageMutator> regularPageMutator;
// Mutator for incognito Tabs.
@property(nonatomic, weak) id<TabGridPageMutator> incognitoPageMutator;
// Mutator for Tab Groups.
@property(nonatomic, weak) id<TabGridPageMutator> tabGroupsPageMutator;

// Mutator to handle toolbars modification.
@property(nonatomic, weak) id<GridToolbarsMutator> toolbarsMutator;

// Consumer for state changes in tab grid.
@property(nonatomic, weak) id<TabGridConsumer> consumer;

- (instancetype)initWithIdentityManager:
                    (signin::IdentityManager*)identityManager
                            prefService:(PrefService*)prefService
               featureEngagementTracker:(feature_engagement::Tracker*)tracker
                             modeHolder:(TabGridModeHolder*)modeHolder
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Set the active page (incognito, regular, or tab groups).
- (void)setActivePage:(TabGridPage)page;
// Stops mediating and disconnects from backend models.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GRID_MEDIATOR_H_
