// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_

#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_delegate.h"

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

// Return the credit card associated with the backend identifier.
- (autofill::CreditCard*)creditCardForIdentifier:(NSString*)identifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_
