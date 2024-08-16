// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MODE_HOLDER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MODE_HOLDER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

@protocol TabGridModeObserving;

// Holder for the TabGrid mode.
@interface TabGridModeHolder : NSObject

// The current mode of the TabGrid.
@property(nonatomic, assign) TabGridMode mode;

// Adding/removing observers.
- (void)addObserver:(id<TabGridModeObserving>)observer;
- (void)removeObserver:(id<TabGridModeObserving>)observer;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MODE_HOLDER_H_
