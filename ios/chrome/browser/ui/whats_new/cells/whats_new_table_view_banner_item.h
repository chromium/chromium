// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_CELLS_WHATS_NEW_TABLE_VIEW_BANNER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_CELLS_WHATS_NEW_TABLE_VIEW_BANNER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// WhatsNewTableViewBannerItem is a model class that uses
// WhatsNewTableViewBannerCell.
@interface WhatsNewTableViewBannerItem : TableViewItem

// The title of the section.
@property(nonatomic, copy) NSString* sectionTitle;

// The title text string.
@property(nonatomic, copy) NSString* title;

// The detail text string.
@property(nonatomic, copy) NSString* detailText;

// The image banner being displayed.
@property(nonatomic, strong) UIImage* bannerImage;

// Boolean to display the banner at the bottom or keep it at the top.
@property(nonatomic, assign) BOOL isBannerAtBottom;

@end

// WhatsNewTableViewBannerCell implements a TableViewCell subclass. The cell is
// laid out vertically containing a large banner image at the top or bottom of
// the cell, and three text labels (section text, text, detail text).
@interface WhatsNewTableViewBannerCell : TableViewCell

// UIImageView that will display the banner image at the top or bottom of the
// cell.
@property(nonatomic, strong) UIImageView* bannerImageView;

// UILabel corresponding to the title from the item.
@property(nonatomic, strong) UILabel* textLabel;

// UILabel corresponding to the detailText from the item.
@property(nonatomic, strong) UILabel* detailTextLabel;

// UILabel corresponding to the sectionTitle from the item.
@property(nonatomic, strong) UILabel* sectionTextLabel;

// Method to update some constraints and the stack view to put the banner image
// at the bottom of the cell.
- (void)setBannerImageAtBottom;

// Method to update some constraints and the stack view with no banner image.
- (void)setEmptyBannerImage;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_CELLS_WHATS_NEW_TABLE_VIEW_BANNER_ITEM_H_
