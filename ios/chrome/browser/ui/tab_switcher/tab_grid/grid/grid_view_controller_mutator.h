// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_MUTATOR_H_

#import <UIKit/UIKit.h>

// Reflects user’s change in grid's model.
@protocol GridViewControllerMutator <NSObject>

// Notifies the model that the number of selected items and the number of
// shareabled one changed.
- (void)numberOfSelectedItemsAndShareableItemsChanged;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_MUTATOR_H_
