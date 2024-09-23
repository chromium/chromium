// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_HANDLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_HANDLER_H_

#import "ios/chrome/browser/autofill/model/credit_card/credit_card_data.h"

// Handler for the payments bottom sheet's context menu.
@protocol PaymentsSuggestionBottomSheetHandler

// Displays the payment methods menu.
- (void)displayPaymentMethods;

// Displays the payment details menu.
- (void)displayPaymentDetailsForCreditCardIdentifier:
    (NSString*)creditCardIdentifier;

// Handles tapping the primary button. The selected credit card's backend
// identifier must be provided. `index` represents the position of the card
// among the available card suggestions.
- (void)primaryButtonTappedForCard:(CreditCardData*)creditCardData
                           atIndex:(NSInteger)index;

// Handles tapping the secondary button.
- (void)secondaryButtonTapped;

// Handles the view disappearing.
- (void)viewDidDisappear;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_HANDLER_H_
