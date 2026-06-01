// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_PAYMENTS_UI_CREDIT_CARD_SUGGESTION_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_PAYMENTS_UI_CREDIT_CARD_SUGGESTION_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/autofill/payments/ui/credit_card_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller.h"

@protocol CreditCardSuggestionBottomSheetDelegate;
@protocol CreditCardSuggestionBottomSheetHandler;

class GURL;

// Payments Bottom Sheet UI, which includes a table to display payments
// suggestions, a button to use a suggestion and a button to revert to
// using the keyboard to enter the payment information.
@interface CreditCardSuggestionBottomSheetViewController
    : TableViewBottomSheetViewController <
          CreditCardSuggestionBottomSheetConsumer>

// Initialize with the delegate used to open payments methods and the URL of the
// current page.
- (instancetype)initWithHandler:
                    (id<CreditCardSuggestionBottomSheetHandler>)handler
                            URL:(const GURL&)URL;

// The delegate for the bottom sheet view controller.
@property(nonatomic, strong) id<CreditCardSuggestionBottomSheetDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_PAYMENTS_UI_CREDIT_CARD_SUGGESTION_BOTTOM_SHEET_VIEW_CONTROLLER_H_
