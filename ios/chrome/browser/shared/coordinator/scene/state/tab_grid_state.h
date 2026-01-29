// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_TAB_GRID_STATE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_TAB_GRID_STATE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"

class TabGroup;

// Protocol for observers of the tab grid state.
@protocol TabGridStateObserver <NSObject>

@optional

// Called right before entering the tab grid.
- (void)willEnterTabGrid;

// Called right before exiting the tab grid.
- (void)willExitTabGrid;

// Called right before the tab grid is changing its page.
- (void)willChangePageTo:(TabGridPage)page;

// Called right before a `group` is shown.
- (void)willShowTabGroup:(const TabGroup*)group;

// Called right before a group is hidden.
- (void)willHideTabGroup;

@end

// Object containing the state of the tab grid.
@interface TabGridState : NSObject

// The page currently displayed on the TabGrid. Updating this property only
// notify the observers. It doesn't impact the state of the TabGrid.
@property(nonatomic, assign) TabGridPage currentPage;

// The page that was used to enter the TabGrid (incognito or regular).
@property(nonatomic, assign) TabGridPage originPage;

// Whether the TabGrid is currently visible. Updating this property only
// notify the observers. It doesn't impact the state of the TabGrid.
@property(nonatomic, assign) BOOL tabGridVisible;

// The tab group currently shown in the tab grid. `nullptr` if no tab group is
// visible.
@property(nonatomic, assign) const TabGroup* visibleTabGroup;

// Adds observer.
- (void)addObserver:(id<TabGridStateObserver>)observer;
// Removes observer.
- (void)removeObserver:(id<TabGridStateObserver>)observer;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_TAB_GRID_STATE_H_
