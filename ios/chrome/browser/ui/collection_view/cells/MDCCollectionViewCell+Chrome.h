// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_MDCCOLLECTIONVIEWCELL_CHROME_H_
#define IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_MDCCOLLECTIONVIEWCELL_CHROME_H_

#import <CoreGraphics/CoreGraphics.h>

#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

@class CollectionViewItem;

// Augments MDCCollectionViewCell for use in conjunction with
// CollectionViewItem.
@interface MDCCollectionViewCell (Chrome)

// Clears global cache that holds template cells for sizing. It is needed
// to handle a11y dynamic type fonts.
+ (void)cr_clearPreferredHeightForWidthCellCache;

// Returns the height this class of cell would need to be to fit within
// |targetWidth|, configured by |item|. The returned height is calculated by
// Auto Layout so that the contents of the cell could fit within the
// |targetWidth|.
// If the cell contains multi-line labels, make sure to update the
// |preferredMaxLayoutWidth| in -layoutSubviews like so:
//
// @implementation MyCell
//
// - (void)layoutSubView {
//   [super layoutSubviews];
//
//   // Adjust the text label preferredMaxLayoutWidth when the parent's width
//   // changes, for instance on screen rotation.
//   CGFloat parentWidth = CGRectGetWidth(self.contentView.bounds);
//   _textLabel.preferredMaxLayoutWidth = parentWidth - 2 * kHorizontalPadding;
//
//   // Re-layout with the new preferred width to allow the label to adjust its
//   // height.
//   [super layoutSubviews];
// }
//
// @end
+ (CGFloat)cr_preferredHeightForWidth:(CGFloat)targetWidth
                              forItem:(CollectionViewItem*)item;

// Sets the accessory type and sets its view tint color to match Chrome's
// settings colors.
- (void)cr_setAccessoryType:(MDCCollectionViewCellAccessoryType)accessoryType;

@end

#endif  // IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_MDCCOLLECTIONVIEWCELL_CHROME_H_
