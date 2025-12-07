// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_ITEM_CELL_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_ITEM_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/public/composebox_theme.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"

@class ComposeboxInputItemCell;

// Delegate for ComposeboxInputItemCell.
@protocol ComposeboxInputItemCellDelegate <NSObject>
// Called when the close button is tapped.
- (void)composeboxInputItemCellDidTapCloseButton:(ComposeboxInputItemCell*)cell;

@end

// A versatile cell for displaying any ComposeboxInputItem.
@interface ComposeboxInputItemCell : UICollectionViewCell

// The delegate for the cell.
@property(nonatomic, weak) id<ComposeboxInputItemCellDelegate> delegate;

// Configures the cell with the given item.
- (void)configureWithItem:(ComposeboxInputItem*)item
                    theme:(ComposeboxTheme*)theme;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_ITEM_CELL_H_
