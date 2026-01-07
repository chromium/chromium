// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_TAB_GRID_STATE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_TAB_GRID_STATE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"

// Protocol for observers of the tab grid state.
@protocol TabGridStateObserver <NSObject>

@optional

// Called right before entering the tab grid.
- (void)willEnterTabGrid;

// Called right before exiting the tab grid.
- (void)willExitTabGrid;

// Called right before the tab grid is changing its page.
- (void)willChangePageTo:(TabGridPage)page;

@end

// Object containing the state of the tab grid.
@interface TabGridState : NSObject

// The page currently displayed on the TabGrid. Updating this property only
// notify the observers. It doesn't impact the state of the TabGrid.
@property(nonatomic, assign) TabGridPage currentPage;

// Whether the TabGrid is currently visible. Updating this property only
// notify the observers. It doesn't impact the state of the TabGrid.
@property(nonatomic, assign) BOOL tabGridVisible;

// Adds observer.
- (void)addObserver:(id<TabGridStateObserver>)observer;
// Removes observer.
- (void)removeObserver:(id<TabGridStateObserver>)observer;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_TAB_GRID_STATE_H_
