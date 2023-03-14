// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_CELLS_TABLE_VIEW_STACKED_DETAILS_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_CELLS_TABLE_VIEW_STACKED_DETAILS_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// TableViewStackedDetailsItem contains the model data for a
// TableViewStackedDetailsCell.
@interface TableViewStackedDetailsItem : TableViewItem

// Text for the title of the cell.
@property(nonatomic, copy) NSString* titleText;

// Texts displayed in the details labels of the cell.
@property(nonatomic, copy) NSArray<NSString*>* detailTexts;

// Text color for the details labels of the cell. Default is [UIColor
// colorNamed:kTextPrimaryColor].
@property(nonatomic, strong) UIColor* detailTextColor;

@end

// TableViewCell displaying a title and multiple single-line details texts.
@interface TableViewStackedDetailsCell : TableViewCell

@property(nonatomic, strong, readonly) UILabel* titleLabel;
@property(nonatomic, strong, readonly) NSArray<UILabel*>* detailLabels;

- (void)setDetails:(NSArray<NSString*>*)detailTexts;
- (void)setDetailTextColor:(UIColor*)detailTextColor;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_CELLS_TABLE_VIEW_STACKED_DETAILS_ITEM_H_
