// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_FOOTER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_FOOTER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_style.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#include "url/gurl.h"

@class CollectionViewFooterCell;

@protocol CollectionViewFooterLinkDelegate<NSObject>

// Notifies the delegate that the link corresponding to |URL| was tapped in
// |cell|.
- (void)cell:(CollectionViewFooterCell*)cell didTapLinkURL:(GURL)URL;

@end

// CollectionViewFooterItem is the model class corresponding to
// CollectionViewFooterCell.
@interface CollectionViewFooterItem : CollectionViewItem

// TODO(crbug.com/891299) remove when all collection and table views are fixed
// for dynamic types.
// Set to YES to use dynamic font types.
@property(nonatomic, assign) BOOL useScaledFont;

// The main text string.
@property(nonatomic, copy) NSString* text;

// The URL to open when the link in |text| is tapped.
@property(nonatomic, assign) GURL linkURL;

// The delegate to notify when the link in |text| is tapped.
@property(nonatomic, weak) id<CollectionViewFooterLinkDelegate> linkDelegate;

// The image to show.
@property(nonatomic, strong) UIImage* image;

// The style to use for the cell.
@property(nonatomic, assign) CollectionViewCellStyle cellStyle;

@end

// CollectionViewFooterCell implements a UICollectionViewCell subclass
// containing a single text label.  The label is laid to fill the full width of
// the cell and is wrapped as needed to fit in the cell.
@interface CollectionViewFooterCell : MDCCollectionViewCell

// UILabels corresponding to |text| from the item.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// UIImageView corresponding to |image| from the item.
@property(nonatomic, readonly, strong) UIImageView* imageView;

// Padding on leading and trailing edges of cell.
@property(nonatomic, assign) CGFloat horizontalPadding;

@end

#endif  // IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_FOOTER_ITEM_H_
