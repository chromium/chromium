// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_CVC_ITEM_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_CVC_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// Item corresponding to a CVCCell.
@interface CVCItem : CollectionViewItem

// The instructions text to display.
@property(nonatomic, copy) NSString* instructionsText;

// The optional error message to display.
@property(nonatomic, copy) NSString* errorMessage;

// The month text appearing in the |monthInput| of the cell, if |showDateInput|
// is true.
@property(nonatomic, copy) NSString* monthText;

// The year text appearing in the |yearInput| of the cell, if |showDateInput|
// is true.
@property(nonatomic, copy) NSString* yearText;

// The CVC text appearing in the |CVCInput| of the cell.
@property(nonatomic, copy) NSString* CVCText;

// Whether the cell should show the date inputs.
@property(nonatomic, assign) BOOL showDateInput;

// Whether the cell should show the "New Card?" button.
@property(nonatomic, assign) BOOL showNewCardButton;

// Whether the CVC input contains erroneous data.
@property(nonatomic, assign) BOOL showCVCInputError;

// The resource ID of the CVC image to use.
@property(nonatomic, assign) int CVCImageResourceID;

@end

// The CVC cell includes a label with instructions, optional text fields that
// allow the user to enter a new expiration date, a text field where the user
// enters the CVC, and an image showing where the CVC appears on the card (which
// varies by card type). If there is an error message but a new expiration date
// is not requested, a "New card?" link will be shown that allows the user to
// show the date input fields. Below the inputs, the error messsage will be
// shown if supplied.
@interface CVCCell : MDCCollectionViewCell

// The label displaying instructions.
@property(nonatomic, readonly, strong) UILabel* instructionsTextLabel;

// The label displaying the optional error.
@property(nonatomic, readonly, strong) UILabel* errorLabel;

// The text field for entering the month.
@property(nonatomic, readonly, strong) UITextField* monthInput;

// The text field for entering the year.
@property(nonatomic, readonly, strong) UITextField* yearInput;

// The text field for entering the CVC.
@property(nonatomic, readonly, strong) UITextField* CVCInput;

// The image view to display the CVC image with the |CVCResourceID|.
@property(nonatomic, readonly, strong) UIImageView* CVCImageView;

// The "New Card?" button.
@property(nonatomic, readonly, strong) UIButton* buttonForNewCard;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_CVC_ITEM_H_
