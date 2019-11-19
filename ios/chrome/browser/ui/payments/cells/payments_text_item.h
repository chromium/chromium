// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_CELLS_PAYMENTS_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_CELLS_PAYMENTS_TEXT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_is_selectable.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// The possible types a PaymentsTextCell can have.
typedef NS_ENUM(NSUInteger, PaymentsTextCellType) {
  // The cell has a normal type.
  PaymentsTextCellTypeNormal,
  // The cell has a call to action type.
  PaymentsTextCellTypeCallToAction,
};

// PaymentsTextItem is the model class corresponding to PaymentsTextCell.
@interface PaymentsTextItem : CollectionViewItem<PaymentsIsSelectable>

// The main text to display.
@property(nonatomic, nullable, copy) NSString* text;

// The secondary text to display.
@property(nonatomic, nullable, copy) NSString* detailText;

// The color of the main text.
@property(nonatomic, null_resettable, copy) UIColor* textColor;

// The color of the secondary text.
@property(nonatomic, null_resettable, copy) UIColor* detailTextColor;

// The leading image to display.
@property(nonatomic, nullable, strong) UIImage* leadingImage;

// The tint color for the leading image.
@property(nonatomic, nullable, strong) UIColor* leadingImageTintColor;

// The trailing image to display.
@property(nonatomic, nullable, strong) UIImage* trailingImage;

// The tint color for the trailing image.
@property(nonatomic, nullable, strong) UIColor* trailingImageTintColor;

// The accessory type for the represented cell.
@property(nonatomic) MDCCollectionViewCellAccessoryType accessoryType;

// The type of the represented cell.
@property(nonatomic) PaymentsTextCellType cellType;

@end

// PaymentsTextCell implements a MDCCollectionViewCell subclass containing
// a main text label, a secondary text label and two optional images (one
// leading and one trailing). The labels are laid out to fill the full width of
// the cell and are wrapped as needed to fit in the cell. One image is laid out
// on the leading edge of the cell, and the other on the trailing edge of the
// cell. The text labels are laid out on the the trailing edge of the leading
// image, if one exists, or the leading edge of the cell otherwise, up to the
// leading edge of the trailing image, if one exists, or the trailing edge of
// the cell otherwise.
@interface PaymentsTextCell : MDCCollectionViewCell

// UILabel corresponding to |text| from the item.
@property(nonatomic, readonly, nullable, strong) UILabel* textLabel;

// UILabel corresponding to |detailText| from the item.
@property(nonatomic, readonly, nullable, strong) UILabel* detailTextLabel;

// UIImageView corresponding to |leadingImage| from the item.
@property(nonatomic, readonly, nullable, strong) UIImageView* leadingImageView;

// UIImageView corresponding to |trailingImage| from the item.
@property(nonatomic, readonly, nullable, strong) UIImageView* trailingImageView;

// The type of the cell.
@property(nonatomic) PaymentsTextCellType cellType;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_CELLS_PAYMENTS_TEXT_ITEM_H_
