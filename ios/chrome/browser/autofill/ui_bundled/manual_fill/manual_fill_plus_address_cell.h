// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PLUS_ADDRESS_CELL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PLUS_ADDRESS_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@class FaviconAttributes;
@class FaviconView;
class GURL;
@protocol ManualFillContentInjector;
@class ManualFillPlusAddress;

// Wrapper to show plus address cells in a LegacyChromeTableViewController.
@interface ManualFillPlusAddressItem : TableViewItem

// URL to fetch the favicon.
@property(nonatomic, readonly) const GURL& faviconURL;

// Identifier to match a URLItem with its URLCell.
@property(nonatomic, readonly) NSString* uniqueIdentifier;

// Plus address associated with this item.
@property(nonatomic, readonly) NSString* plusAddress;

// Inits a plus address with an `plusAddress`, a `contentInjector` and
// `menuActions` for user selection. `cellIndexAccessibilityLabel` is the cell's
// accessibility label and is used to indicate the index at which the plus
// address represented by this item is positioned in the list of plus addresses
// to show.
- (instancetype)initWithPlusAddress:(ManualFillPlusAddress*)plusAddress
                    contentInjector:
                        (id<ManualFillContentInjector>)contentInjector
                        menuActions:(NSArray<UIAction*>*)menuActions
        cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@end

// Cell to display a plus address into parts that are interactable
// and sendable the data to the delegate.
@interface ManualFillPlusAddressCell : TableViewCell

// Identifier to match a URLItem with its URLCell.
@property(nonatomic, readonly) NSString* uniqueIdentifier;

// Updates the cell with a `plusAddress`, a `contentInjector` to be notified and
// `menuActions` to set up an overflow menu. `cellIndexAccessibilityLabel` is
// this cell's accessibility label and is used to indicate the index at which
// the plus address represented by this cell is positioned in the list of plus
// addresses to show.
- (void)setUpWithPlusAddress:(ManualFillPlusAddress*)plusAddress
                contentInjector:(id<ManualFillContentInjector>)contentInjector
                    menuActions:(NSArray<UIAction*>*)menuActions
    cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel;

// Configures the cell for the passed favicon attributes.
- (void)configureWithFaviconAttributes:(FaviconAttributes*)attributes;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PLUS_ADDRESS_CELL_H_
