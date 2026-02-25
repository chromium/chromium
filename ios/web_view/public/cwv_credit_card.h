// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_H_

#import <UIKit/UIKit.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Represents the record type of the credit card.
typedef NS_ENUM(NSInteger, CWVCreditCardRecordType) {
  CWVCreditCardRecordTypeLocalCard,
  CWVCreditCardRecordTypeMaskedServerCard,
  CWVCreditCardRecordTypeFullServerCard,
  CWVCreditCardRecordTypeVirtualCard,
};

// Represents a credit card for autofilling payment forms.
CWV_EXPORT
@interface CWVCreditCard : NSObject

// The full name of the card holder. e.g. "John Doe".
@property(nonatomic, copy, nullable, readonly) NSString* cardHolderFullName;
// The permanent account number of the card. e.g. "0123456789012345".
@property(nonatomic, copy, nullable, readonly) NSString* cardNumber;
// The CVC (Card Verification Code) for this card. e.g. "123".
// Only available for unmasked server/virtual cards.
@property(nonatomic, copy, nullable, readonly) NSString* CVC;
// The network this card belongs to. e.g. "Visa", "Amex", "MasterCard".
// Inferred from |cardNumber|.
@property(nonatomic, copy, nullable, readonly) NSString* networkName;
// The image that represents the |networkName|.
@property(nonatomic, readonly) UIImage* networkIcon;
// The month this card expires on. e.g. "08".
@property(nonatomic, copy, nullable, readonly) NSString* expirationMonth;
// The year this card expires on. e.g. "2020".
@property(nonatomic, copy, nullable, readonly) NSString* expirationYear;
// The issuing bank of this card. "Chase", "Bank of America".
@property(nonatomic, copy, nullable, readonly) NSString* bankName;
// The name of the card to be displayed. It will be nickname if available,
// or product description, or network if neither are available.
@property(nonatomic, copy, nullable, readonly) NSString* cardNameForDisplay;
// The record type of this card.
@property(nonatomic, readonly) CWVCreditCardRecordType recordType;
// Whether or not this is a virtual card.
@property(nonatomic, readonly) BOOL isVirtual;
// The unique identifier for this card.
@property(nonatomic, copy, readonly) NSString* GUID;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_H_
