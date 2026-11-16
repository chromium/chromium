// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_ADDRESS_CELL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_ADDRESS_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@class ManualFillAddress;
@protocol ManualFillAddressCellMutator;

// Wrapper to show address cells in a LegacyChromeTableViewController.
@interface ManualFillAddressItem : TableViewItem

// Inits an address with an `address`, a `mutator`, and `menuActions` for user
// selection. `cellIndex` indicates the index (0-based) at which the address
// represented by this item is positioned in the list of addresses to show.
// `cellIndexAccessibilityLabel` is the cell's accessibility label and is used
// to indicate the cell's index (1-based) and the number of available addresses
// to accessibility users.
- (instancetype)initWithAddress:(ManualFillAddress*)address
                        mutator:(id<ManualFillAddressCellMutator>)mutator
                    menuActions:(NSArray<UIAction*>*)menuActions
                      cellIndex:(NSInteger)cellIndex
    cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
         showAutofillFormButton:(BOOL)showAutofillFormButton
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@end

// Cell to display an Address where the fields are interactable
// and send data to the mutator.
@interface ManualFillAddressCell : LegacyTableViewCell

// Updates the cell with an `address`, a `mutator` to be notified, and
// `menuActions` to set up an overflow menu. `cellIndex` indicates the index
// (0-based) at which the address represented by this cell is positioned in the
// list of addresses to show. `cellIndexAccessibilityLabel` is the cell's
// accessibility label and is used to indicate the cell's index (1-based) and
// the number of available addresses to accessibility users.
- (void)setUpWithAddress:(ManualFillAddress*)address
                        mutator:(id<ManualFillAddressCellMutator>)mutator
                    menuActions:(NSArray<UIAction*>*)menuActions
                      cellIndex:(NSInteger)cellIndex
    cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
         showAutofillFormButton:(BOOL)showAutofillFormButton;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_ADDRESS_CELL_H_
