// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CELLS_TABLE_VIEW_ACCOUNT_ITEM_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CELLS_TABLE_VIEW_ACCOUNT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@protocol SystemIdentity;

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

// Those properties should be set before the cell is displayed because
// updates to these will not be reflected after it is shown.
@property(nonatomic, strong) UIImage* image;
@property(nonatomic, copy) NSString* text;
@property(nonatomic, copy) NSString* detailText;
// If this is set to true, an enterprise building icon will be shown on the
// cell.
@property(nonatomic, assign) BOOL managed;

// Set to YES to display an error icon at the end of the cell content with an
// accessibility error message.
@property(nonatomic, assign) BOOL shouldDisplayError;

@property(nonatomic, strong) id<SystemIdentity> identity;
// The default value is TableViewAccountModeEnabled.
@property(nonatomic, assign) TableViewAccountMode mode;

@end

// Cell for account avatar with a leading avatar imageView, title text label,
// and detail text label. This looks very similar to the
// TableViewDetailCell, except that it applies a circular mask to the
// imageView. The imageView is vertical-centered and leading aligned.
// If item/cell is disabled the image and text alpha will be set to 0.5 and
// user interaction will be disabled.
@interface TableViewAccountCell : LegacyTableViewCell

// Rounded image used for the account user picture. On the leading side of the
// cell.
@property(nonatomic, readonly, strong) UIImageView* imageView;
// Cell title.
@property(nonatomic, readonly, strong) UILabel* textLabel;
// Cell subtitle.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

// Set the status view on the trailing side of the cell. `statusView` can be
// nil. It is similar to what `accessoryView` is supposed to be, but does not
// bug on iOS 18. (crbug.com/375377471)
- (void)setStatusView:(UIView*)statusView;

// Same as `setStatusView`, except the argument is an image.
- (void)setStatusViewWithImage:(UIImage*)statusImage;

// Shows or hides the enterprise building icon.
- (void)showManagementIcon:(BOOL)show;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CELLS_TABLE_VIEW_ACCOUNT_ITEM_H_
