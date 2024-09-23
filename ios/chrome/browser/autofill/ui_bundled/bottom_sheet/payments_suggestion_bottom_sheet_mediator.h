// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/payments_suggestion_bottom_sheet_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/payments_suggestion_bottom_sheet_exit_reason.h"

#import <Foundation/Foundation.h>

namespace autofill {
class CreditCard;
struct FormActivityParams;
class PersonalDataManager;
}  // namespace autofill

class WebStateList;

@protocol PaymentsSuggestionBottomSheetConsumer;
// This mediator fetches a list suggestions to display in the bottom sheet.
// It also manages filling the form when a suggestion is selected, as well
// as showing the keyboard if requested when the bottom sheet is dismissed.
@interface PaymentsSuggestionBottomSheetMediator
    : NSObject <PaymentsSuggestionBottomSheetDelegate>

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                              params:(const autofill::FormActivityParams&)params
                 personalDataManager:
                     (autofill::PersonalDataManager*)personalDataManager;

// The bottom sheet suggestions consumer.
@property(nonatomic, weak) id<PaymentsSuggestionBottomSheetConsumer> consumer;

// Whether the bottom sheet has any credit cards to display.
@property(nonatomic, readonly) BOOL hasCreditCards;

// Disconnects the mediator.
- (void)disconnect;

// Returns the credit card associated with the backend identifier, if any.
- (std::optional<autofill::CreditCard>)creditCardForIdentifier:
    (NSString*)identifier;

// Logs bottom sheet exit reasons, like dismissal or using a payment method.
- (void)logExitReason:(PaymentsSuggestionBottomSheetExitReason)exitReason;

// Sends the information about which credit card from the bottom sheet was
// selected by the user, which is expected to fill the relevant fields. `index`
// represents the position of the selected card in the list of card suggestions.
- (void)didSelectCreditCard:(CreditCardData*)creditCardData
                    atIndex:(NSInteger)index;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_
