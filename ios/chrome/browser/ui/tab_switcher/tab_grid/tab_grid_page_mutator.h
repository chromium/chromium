// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_PAGE_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_PAGE_MUTATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

// Allows the tab grid mediator to reflect user’s change in grid's model.
@protocol TabGridPageMutator <NSObject>

// Notifies the model that the user changed the grid. `selected` is set to YES
// if the grid is currently the one selected by the user.
- (void)currentlySelectedGrid:(BOOL)selected;

// Notifies the model that the current page is the active one.
- (void)setPageAsActive;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_PAGE_MUTATOR_H_
