// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_SUGGESTED_ACTIONS_SUGGESTED_ACTIONS_GRID_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_SUGGESTED_ACTIONS_SUGGESTED_ACTIONS_GRID_CELL_H_

#import <UIKit/UIKit.h>

// A cell in a grid that contains a table of suggested actions.
@interface SuggestedActionsGridCell : UICollectionViewCell
// The view to be added in the content of the cell.
@property(nonatomic, weak) UIView* suggestedActionsView;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_SUGGESTED_ACTIONS_SUGGESTED_ACTIONS_GRID_CELL_H_
