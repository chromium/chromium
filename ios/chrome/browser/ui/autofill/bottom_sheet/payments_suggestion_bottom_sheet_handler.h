// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_HANDLER_H_

// Handler for the payments bottom sheet's context menu.
@protocol PaymentsSuggestionBottomSheetHandler

// Displays the payment methods menu.
- (void)displayPaymentMethods;

// Displays the payment details menu.
- (void)displayPaymentDetailsForCreditCardIdentifier:
    (NSString*)creditCardIdentifier;

// Handles tapping the primary button. The selected credit card's backend
// identifier must be provided.
- (void)primaryButtonTapped:(NSString*)backendIdentifier;

// Handles tapping the secondary button.
- (void)secondaryButtonTapped;

// Handles the view disappearing.
- (void)viewDidDisappear:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_HANDLER_H_
