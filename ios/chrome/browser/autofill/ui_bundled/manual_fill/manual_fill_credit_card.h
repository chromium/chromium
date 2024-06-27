// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CREDIT_CARD_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CREDIT_CARD_H_

#import <Foundation/Foundation.h>
#import "components/autofill/core/browser/data_model/credit_card.h"

// This represents a credit card to use with manual fill.
@interface ManualFillCreditCard : NSObject

// The credit card GUID, for match with c++ version.
@property(nonatomic, readonly) NSString* GUID;

// The provider network related to this credit card.
@property(nonatomic, readonly) NSString* network;

// The bank related to this credit card. Can be nil.
@property(nonatomic, readonly) NSString* bankName;

// The credit card holder name.
@property(nonatomic, readonly) NSString* cardHolder;

// The credit card number. Can be nil if the card is masked and requires
// unlocking though CVC input.
@property(nonatomic, readonly) NSString* number;

// The credit card number obfuscated for display purpose.
@property(nonatomic, readonly) NSString* obfuscatedNumber;

// A label for this card formatted as 'IssuerNetwork ****2345'. Used to identify
// this card in tests.
@property(nonatomic, readonly) NSString* networkAndLastFourDigits;

// The credit card expiration year.
@property(nonatomic, readonly) NSString* expirationYear;

// The credit card expiration month.
@property(nonatomic, readonly) NSString* expirationMonth;

// The credit card CVC. Can be nil for non-virtual cards.
@property(nonatomic, readonly) NSString* CVC;

// The credit card icon.
@property(nonatomic, readonly) UIImage* icon;

// The type of card: masked, local, virtual, etc.
@property(nonatomic, readonly) autofill::CreditCard::RecordType recordType;

// YES if the card can be filled directly from the ManualFillCreditCard NO if it
// needs to be requested.
@property(nonatomic, readonly) BOOL canFillDirectly;

// Default init. `GUID` and `number` are the only fields considered for
// equality, so we can differentiate between an obfuscated and a complete one.
- (instancetype)initWithGUID:(NSString*)GUID
                     network:(NSString*)network
                        icon:(UIImage*)icon
                    bankName:(NSString*)bankName
                  cardHolder:(NSString*)cardHolder
                      number:(NSString*)number
            obfuscatedNumber:(NSString*)obfuscatedNumber
    networkAndLastFourDigits:(NSString*)networkAndLastFourDigits
              expirationYear:(NSString*)expirationYear
             expirationMonth:(NSString*)expirationMonth
                         CVC:(NSString*)CVC
                  recordType:(autofill::CreditCard::RecordType)recordType
             canFillDirectly:(BOOL)canFillDirecly NS_DESIGNATED_INITIALIZER;

// Unavailable. Please use `initWithGuid:network:bankName:cardholder:number:
// obfuscatedNumber:expirationYear:expirationMonth:`.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CREDIT_CARD_H_
