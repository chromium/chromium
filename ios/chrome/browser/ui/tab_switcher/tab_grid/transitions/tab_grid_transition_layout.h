// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_LAYOUT_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_LAYOUT_H_

#import <UIKit/UIKit.h>

@class TabGridTransitionItem;

// Transition layout for the tab grid.
@interface TabGridTransitionLayout : NSObject

// Active cell transition item.
@property(nonatomic, readonly) TabGridTransitionItem* activeCell;

// Creates a new TabGridTransitionLayout instance with the given `activeCell`.
+ (instancetype)layoutWithActiveCell:(TabGridTransitionItem*)activeCell;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_LAYOUT_H_
