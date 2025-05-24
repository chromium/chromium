// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_CELLS_PLUS_ADDRESS_SUGGESTION_LABEL_CELL_H_
#define IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_CELLS_PLUS_ADDRESS_SUGGESTION_LABEL_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"

@protocol PlusAddressSuggestionLabelCellDelegate <NSObject>

// Notifies the delegate that the button was pressed.
- (void)didTapTrailingButton;

@end

// Cell used to layout the row in the table view in the following format:
//
// +------------------------------------------------------------------+
// | `leadingImage`         `label`          `optional trailingImage` |
// +------------------------------------------------------------------+
//
@interface PlusAddressSuggestionLabelCell : TableViewCell

// Delegate to notify when the button is tapped.
@property(nonatomic, weak) id<PlusAddressSuggestionLabelCellDelegate> delegate;

// Sets the `trailingButtonImage` (and its tint `tintColor`) that should be
// displayed at the trailing edge of the cell.
- (void)setTrailingButtonImage:(UIImage*)trailingButtonImage
                 withTintColor:(UIColor*)tintColor
       accessibilityIdentifier:(NSString*)accessibilityIdentifier;

// Sets the `image` that should be displayed at the leading edge of the cell
// with a `tintColor`.
- (void)setLeadingIconImage:(UIImage*)image withTintColor:(UIColor*)tintColor;

// Called to show or hide the activity indicators.
- (void)showActivityIndicator;
- (void)hideActivityIndicator;

@end

#endif  // IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_CELLS_PLUS_ADDRESS_SUGGESTION_LABEL_CELL_H_
