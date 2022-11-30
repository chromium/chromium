// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CARD_CELL_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CARD_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_credit_card.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

@protocol CardListDelegate;
@protocol ManualFillContentInjector;

// Wrapper to show card cells in a ChromeTableViewController.
@interface ManualFillCardItem : TableViewItem

- (instancetype)initWithCreditCard:(ManualFillCreditCard*)card
                   contentInjector:
                       (id<ManualFillContentInjector>)contentInjector
                navigationDelegate:(id<CardListDelegate>)navigationDelegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@end

// Cell to display a Card where the username and password are interactable
// and send the data to the delegate.
@interface ManualFillCardCell : TableViewCell

// Updates the cell with credit card and the `delegate` to be notified.
- (void)setUpWithCreditCard:(ManualFillCreditCard*)card
            contentInjector:(id<ManualFillContentInjector>)contentInjector
         navigationDelegate:(id<CardListDelegate>)navigationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CARD_CELL_H_
