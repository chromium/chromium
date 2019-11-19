// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_TABLE_VIEW_ACCOUNT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_TABLE_VIEW_ACCOUNT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

@class ChromeIdentity;

typedef NS_ENUM(NSInteger, TableViewAccountMode) {
  // The cell can be tappable, and the colors are not dimmed.
  TableViewAccountModeEnabled,
  // The cell is not tappable, and the colors are not dimmed.
  TableViewAccountModeNonTappable,
  // The cell is not tappable, and the colors are dimmed.
  TableViewAccountModeDisabled,
};

// Item for account avatar, used everywhere an account cell is shown.
@interface TableViewAccountItem : TableViewItem

@property(nonatomic, strong) UIImage* image;
@property(nonatomic, copy) NSString* text;
@property(nonatomic, copy) NSString* detailText;
@property(nonatomic, assign) BOOL shouldDisplayError;
@property(nonatomic, strong) ChromeIdentity* chromeIdentity;
// The default value is TableViewAccountModeEnabled.
@property(nonatomic, assign) TableViewAccountMode mode;

@end

// Cell for account avatar with a leading avatar imageView, title text label,
// and detail text label. This looks very similar to the
// TableViewDetailCell, except that it applies a circular mask to the
// imageView. The imageView is vertical-centered and leading aligned.
// If item/cell is disabled the image and text alpha will be set to 0.5 and
// user interaction will be disabled.
@interface TableViewAccountCell : TableViewCell

// Rounded image used for the account user picture.
@property(nonatomic, readonly, strong) UIImageView* imageView;
// Cell title.
@property(nonatomic, readonly, strong) UILabel* textLabel;
// Cell subtitle.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;
// Error icon that will be displayed on the left side of the cell.
@property(nonatomic, readonly, strong) UIImageView* errorIcon;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_TABLE_VIEW_ACCOUNT_ITEM_H_
