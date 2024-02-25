// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_CELLS_WHATS_NEW_TABLE_VIEW_ITEM_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_CELLS_WHATS_NEW_TABLE_VIEW_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// WhatsNewTableViewItem is a model class that uses WhatsNewTableViewCell.
@interface WhatsNewTableViewItem : TableViewItem

// The title text string.
@property(nonatomic, copy) NSString* title;

// The detail text string.
@property(nonatomic, copy) NSString* detailText;

// The icon background color. If empty, no background would be set for the icon.
@property(nonatomic, copy) UIColor* iconBackgroundColor;

// The leading icon image.
@property(nonatomic, copy) UIImage* iconImage;

@end

// WhatsNewTableViewCell implements an TableViewCell subclass containing a
// leading icon and two text labels (text and detail text) laid out vertically.
@interface WhatsNewTableViewCell : TableViewCell

// UILabels corresponding to `title` from the item.
@property(nonatomic, strong) UILabel* textLabel;

// UILabels corresponding to `detailText` from the item.
@property(nonatomic, strong) UILabel* detailTextLabel;

// UIImageView that will display the leading icon image of the cell.
@property(nonatomic, strong) UIImageView* iconView;

// UIImageView that will display the background color with rounded corners of
// the icon.
@property(nonatomic, strong) UIImageView* iconBackgroundImageView;

// Update the constraints when the image has no background.
- (void)updateImageConstraintWhenNoBackground;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_CELLS_WHATS_NEW_TABLE_VIEW_ITEM_H_
