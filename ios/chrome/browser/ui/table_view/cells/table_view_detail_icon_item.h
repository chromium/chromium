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

// The main text string.
@property(nonatomic, copy) NSString* text;

// The detail text string.
@property(nonatomic, copy) NSString* detailText;

@end

// TableViewDetailIconCell implements an TableViewCell subclass containing an
// optional leading icon and two text labels: a "main" label and a "detail"
// label. The two labels are laid out side-by-side and fill the full width of
// the cell. Labels are truncated as needed to fit in the cell.
@interface TableViewDetailIconCell : TableViewCell

// UILabels corresponding to |text| and |detailText| from the item.
@property(nonatomic, readonly, strong) UILabel* textLabel;
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

// Sets the image that should be displayed at the leading edge of the cell. If
// set to nil, the icon will be hidden and the text labels will expand to fill
// the full width of the cell.
- (void)setIconImage:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DETAIL_ICON_ITEM_H_
