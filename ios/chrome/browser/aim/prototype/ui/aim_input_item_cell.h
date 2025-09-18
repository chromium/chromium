// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_INPUT_ITEM_CELL_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_INPUT_ITEM_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/aim/prototype/ui/aim_input_item.h"

@class AIMInputItemCell;

// Delegate for AIMInputItemCell.
@protocol AIMInputItemCellDelegate <NSObject>
// Called when the close button is tapped.
- (void)aimInputItemCellDidTapCloseButton:(AIMInputItemCell*)cell;
@end

// A versatile cell for displaying any AIMInputItem.
@interface AIMInputItemCell : UICollectionViewCell

// The delegate for the cell.
@property(nonatomic, weak) id<AIMInputItemCellDelegate> delegate;

// Configures the cell with the given item.
- (void)configureWithItem:(AIMInputItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_INPUT_ITEM_CELL_H_
