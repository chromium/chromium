// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_MUTATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_MUTATOR_H_

#import <Foundation/Foundation.h>

#import "components/autofill/core/browser/payments/payments_autofill_client.h"

// Delegate to handle user actions from the save card bottomsheet view
// controller.
@protocol SaveCardBottomSheetMutator <NSObject>

// Handles user accepting the save card bottomsheet through the accept button.
- (void)didAccept;

// Handles user dismissing the save card bottomsheet through the cancel button.
- (void)didCancel;

// Triggered when the user confirms the "Scan and Save" flow with edited
// details.
- (void)onUpdatedAndAcceptedForSaveAndFill:
    (autofill::payments::PaymentsAutofillClient::
         UserProvidedCardSaveAndFillDetails)details;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_MUTATOR_H_
