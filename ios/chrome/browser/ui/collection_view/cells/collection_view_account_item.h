// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_ACCOUNT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_ACCOUNT_ITEM_H_

#import <UIKit/UIKit.h>

#import <MaterialComponents/MaterialCollectionCells.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"

@class ChromeIdentity;

// Item for account avatar, used everywhere an account cell is shown.
@interface CollectionViewAccountItem : CollectionViewItem

@property(nonatomic, strong) UIImage* image;
@property(nonatomic, copy) NSString* text;
@property(nonatomic, copy) NSString* detailText;
@property(nonatomic, assign) BOOL shouldDisplayError;
@property(nonatomic, strong) ChromeIdentity* chromeIdentity;
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

// The accessory type for the represented cell.
@property(nonatomic) MDCCollectionViewCellAccessoryType accessoryType;

@end

// Cell for account avatar with a leading avatar imageView, title text label,
// and detail text label. This looks very similar to the
// MDCCollectionViewDetailCell, except that it applies a circular mask to the
// imageView. The imageView is vertical-centered and leading aligned.
// If item/cell is disabled the image and text alpha will be set to 0.5 and
// user interaction will be disabled.
@interface CollectionViewAccountCell : MDCCollectionViewCell

// Rounded image used for the account user picture.
@property(nonatomic, readonly, strong) UIImageView* imageView;
// Cell title.
@property(nonatomic, readonly, strong) UILabel* textLabel;
// Cell subtitle.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;
// Error icon that will be displayed on the left side of the cell.
@property(nonatomic, readonly, strong) UIImageView* errorIcon;

@end

#endif  // IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_ACCOUNT_ITEM_H_
