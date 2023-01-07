// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_TEXT_CELL_H_
#define IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_TEXT_CELL_H_

#import <UIKit/UIKit.h>

#import <MaterialComponents/MDCCollectionViewCell.h>

// MDCCollectionViewCell that displays two text fields.
@interface CollectionViewTextCell : MDCCollectionViewCell

// The first line of text to display.
@property(nonatomic, readonly, strong, nullable) UILabel* textLabel;

// The second line of detail text to display.
@property(nonatomic, readonly, strong, nullable) UILabel* detailTextLabel;

// Returns the height needed for a cell contained in `width` to display
// `titleLabel` and `detailTextLabel`.
+ (CGFloat)heightForTitleLabel:(nullable UILabel*)titleLabel
               detailTextLabel:(nullable UILabel*)detailTextLabel
                         width:(CGFloat)width;

@end

#endif  // IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_TEXT_CELL_H_
