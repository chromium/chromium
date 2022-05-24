// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DETAIL_ICON_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DETAIL_ICON_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// TableViewDetailIconItem is a model class that uses TableViewDetailIconCell.
@interface TableViewDetailIconItem : TableViewItem

// The filename for the leading icon.  If empty, no icon will be shown.
@property(nonatomic, copy) NSString* iconImageName;

// The symbol leading icon. If empty, no icon will be shown.
@property(nonatomic, copy) UIImage* symbolImage;

// The background color for the leading |symbolImage|.
@property(nonatomic, copy) UIColor* symbolBackgroundColor;

// The main text string.
@property(nonatomic, copy) NSString* text;

// The detail text string.
@property(nonatomic, copy) NSString* detailText;

// The layout constraint axis at which `text` and `detailText` should be
// aligned. In the case of a vertical layout, the text will adapt its font
// size to a title/subtitle style.
// Defaults to UILayoutConstraintAxisHorizontal.
@property(nonatomic, assign) UILayoutConstraintAxis textLayoutConstraintAxis;

@end

// TableViewDetailIconCell implements an TableViewCell subclass containing an
// optional leading icon and two text labels: a "main" label and a "detail"
// label. The layout of the two labels is based on `textLayoutConstraintAxis`
// defined as either (1) horizontally laid out side-by-side and filling the full
// width of the cell or (2) vertically laid out and filling the full height of
// the cell. Labels are truncated as needed to fit in the cell.
@interface TableViewDetailIconCell : TableViewCell

// UILabels corresponding to `text` and `detailText` from the item.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// The layout constraint axis of the text labels within the cell. Defaults
// to a horizontal, edge aligned layout.
@property(nonatomic, readwrite, assign)
    UILayoutConstraintAxis textLayoutConstraintAxis;

// Sets the image that should be displayed at the leading edge of the cell. If
// set to nil, the icon will be hidden and the text labels will expand to fill
// the full width of the cell.
- (void)setIconImage:(UIImage*)image;

// Sets the leading symbol image with an elevated effect and its background
// color. If set to nil, the icon will be hidden and the text labels will
// expand to fill the full width of the cell.
- (void)setSymbolImage:(UIImage*)image
       backgroundColor:(UIColor*)backgroundColor;

// Sets the detail text. `detailText` can be nil (or empty) to hide the detail
// text.
- (void)setDetailText:(NSString*)detailText;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DETAIL_ICON_ITEM_H_
