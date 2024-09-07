// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CARD_CELL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CARD_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credit_card.h"

@protocol CardListDelegate;
@protocol ManualFillContentInjector;

// Wrapper to show card cells in a LegacyChromeTableViewController.
@interface ManualFillCardItem : TableViewItem

// `cellIndex` indicates the index (0-based) at which the payment method
// represented by this item is positioned in the list of payment methods to
// show. `cellIndexAccessibilityLabel` is the cell's accessibility label and is
// used to indicate the cell's index (1-based) and the number of available
// payment methods to accessibility users.
- (instancetype)initWithCreditCard:(ManualFillCreditCard*)card
                   contentInjector:
                       (id<ManualFillContentInjector>)contentInjector
                navigationDelegate:(id<CardListDelegate>)navigationDelegate
                       menuActions:(NSArray<UIAction*>*)menuActions
                         cellIndex:(NSInteger)cellIndex
       cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
            showAutofillFormButton:(BOOL)showAutofillFormButton
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@end

// Cell to display a Card where the username and password are interactable
// and send the data to the delegate.
@interface ManualFillCardCell : TableViewCell

// Updates the cell with credit card and the `navigationDelegate` to be
// notified. `menuActions` are the UIActions that should be available from the
// cell's overflow menu button. `cellIndex` indicates the index (0-based) at
// which the payment method represented by this cell is positioned in the list
// of payment methods to show. `cellIndexAccessibilityLabel` is the cell's
// accessibility label and is used to indicate the cell's index (1-based) and
// the number of available payment methods to accessibility users.
- (void)setUpWithCreditCard:(ManualFillCreditCard*)card
                contentInjector:(id<ManualFillContentInjector>)contentInjector
             navigationDelegate:(id<CardListDelegate>)navigationDelegate
                    menuActions:(NSArray<UIAction*>*)menuActions
                      cellIndex:(NSInteger)cellIndex
    cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
         showAutofillFormButton:(BOOL)showAutofillFormButton;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CARD_CELL_H_
