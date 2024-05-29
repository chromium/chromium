// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ADDRESS_CELL_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ADDRESS_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_address.h"

// TODO(crbug.com/40577448): rename, see
// https://crrev.com/c/1317853/7/ios/chrome/browser/ui/autofill/manual_fill/manual_fill_address_cell.h#17.
@protocol ManualFillContentInjector;

// Wrapper to show address cells in a LegacyChromeTableViewController.
@interface ManualFillAddressItem : TableViewItem

// Inits an address with an `address`, a `contentInjector` and `menuActions` for
// user selection.
- (instancetype)initWithAddress:(ManualFillAddress*)address
                contentInjector:(id<ManualFillContentInjector>)contentInjector
                    menuActions:(NSArray<UIAction*>*)menuActions
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@end

// Cell to display an Address into parts that are interactable
// and sendable the data to the delegate.
@interface ManualFillAddressCell : TableViewCell

// Updates the cell with an `address`, a `contentInjector` to be notified and
// `menuActions` to set up an overflow menu.
- (void)setUpWithAddress:(ManualFillAddress*)address
         contentInjector:(id<ManualFillContentInjector>)contentInjector
             menuActions:(NSArray<UIAction*>*)menuActions;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ADDRESS_CELL_H_
