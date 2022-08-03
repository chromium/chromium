// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ACTION_CELL_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ACTION_CELL_H_

#import <MaterialComponents/MaterialCollectionCells.h>

// Associated cell to display a Most Visited Action tile based.
@interface ContentSuggestionsMostVisitedActionCell : MDCCollectionViewCell

// View for action icon.
@property(nonatomic, strong, readonly, nonnull) UIImageView* iconView;

// Title of the action.
@property(nonatomic, strong, readonly, nonnull) UILabel* titleLabel;

// Container view for `countLabel`.
@property(nonatomic, strong, readonly, nonnull) UIView* countContainer;

// Number shown in circle by top trailing side of cell.
@property(nonatomic, strong, readonly, nonnull) UILabel* countLabel;

// Size for a an action tile.
+ (CGSize)defaultSize;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ACTION_CELL_H_
