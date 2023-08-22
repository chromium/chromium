// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_ILLUSTRATED_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_ILLUSTRATED_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// TableViewItem for the TableView Illustrated Cell.
@interface TableViewIllustratedItem : TableViewItem
// Image being displayed.
@property(nonatomic, strong) UIImage* image;
// Title being displayed under the image.
@property(nonatomic, copy) NSString* title;
// Subtitle being displayed under the title.
@property(nonatomic, copy) NSString* subtitle;
// Text of the button displayed under the subtitle.
@property(nonatomic, copy) NSString* buttonText;
@end

// TableViewCell that displays an image, title, subtitle and button.
@interface TableViewIllustratedCell : TableViewCell
// The imageview that will display the image at the top of the cell.
@property(nonatomic, readonly, strong) UIImageView* illustratedImageView;
// Label displaying the title, underneath the image.
@property(nonatomic, readonly, strong) UILabel* titleLabel;
// Label displaying the subtitle, underneath the title.
@property(nonatomic, readonly, strong) UILabel* subtitleLabel;
// Container of the button that will be displayed under the subtitle. Used to
// provide additional margin for the cell in its parent UIStackView.
@property(nonatomic, readonly, strong) UIView* buttonContainer;
// Button that will be displayed under the subtitle.
@property(nonatomic, readonly, strong) UIButton* button;
@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_ILLUSTRATED_ITEM_H_
