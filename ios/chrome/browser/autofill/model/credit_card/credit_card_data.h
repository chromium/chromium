// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_CREDIT_CARD_CREDIT_CARD_DATA_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_CREDIT_CARD_CREDIT_CARD_DATA_H_

#import <UIKit/UIKit.h>
#import "components/autofill/core/browser/data_model/credit_card.h"

// Data source for each individual credit card.
@interface CreditCardData : NSObject

// The credit card's name and last four digits of the card or the credit card's
// nickname if it has one.
@property(readonly, strong) NSString* cardNameAndLastFourDigits;

// The credit card's expiration date or type.
@property(readonly, strong) NSString* cardDetails;

// The credit card's backend identifier.
@property(readonly, strong) NSString* backendIdentifier;

// The accessible card name description.
@property(readonly, strong) NSString* accessibleCardName;

// The icon associated with this credit card.
@property(readonly, strong) UIImage* icon;

// Returns the record type of the credit card.
@property(readonly) autofill::CreditCard::RecordType recordType;

// Initializes from the autofill credit card type, and an icon.
- (instancetype)initWithCreditCard:(const autofill::CreditCard&)creditCard
                              icon:(UIImage*)icon;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_CREDIT_CARD_CREDIT_CARD_DATA_H_
