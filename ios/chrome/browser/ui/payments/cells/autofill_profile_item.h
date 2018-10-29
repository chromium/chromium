// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_CELLS_AUTOFILL_PROFILE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_CELLS_AUTOFILL_PROFILE_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_is_selectable.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// AutofillProfileItem is the model class corresponding to AutofillProfileCell.
@interface AutofillProfileItem : CollectionViewItem<PaymentsIsSelectable>

// TODO(crbug.com/891299) remove when all collection and table views are fixed
// for dynamic types.
// Set to YES to use dynamic font types.
@property(nonatomic, assign) BOOL useScaledFont;

// Profile's name.
@property(nonatomic, copy) NSString* name;

// Profile's address.
@property(nonatomic, copy) NSString* address;

// Profile's phone number.
@property(nonatomic, copy) NSString* phoneNumber;

// Profile's email address.
@property(nonatomic, copy) NSString* email;

// The notification message.
@property(nonatomic, copy) NSString* notification;

@end

// AutofillProfileItem implements an MDCCollectionViewCell subclass containing
// five optional text labels. Each label is laid out to fill the full width of
// the cell. |nameLabel| and |addressLabel| are wrapped as needed to fit in the
// cell, the rest of the labels are truncated if necessary.
@interface AutofillProfileCell : MDCCollectionViewCell

// UILabels corresponding to |name|, |address|, |phoneNumber|, |email|, and
// |notification|.
@property(nonatomic, readonly, strong) UILabel* nameLabel;
@property(nonatomic, readonly, strong) UILabel* addressLabel;
@property(nonatomic, readonly, strong) UILabel* phoneNumberLabel;
@property(nonatomic, readonly, strong) UILabel* emailLabel;
@property(nonatomic, readonly, strong) UILabel* notificationLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_CELLS_AUTOFILL_PROFILE_ITEM_H_
