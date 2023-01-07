// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_SAVER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_SAVER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVCreditCard;

// Helps with saving a credit card to the user's Google Pay account.
// To make a decision, there are 3 options:
// 1. Call |accept...| to accept the save.
// 2. Call |decline| to decline the save.
// 3. Do nothing and let this instance be deallocated. This is the same as
//    declining, but logs that the user ignored the request.
// Only pick one of these options, and only once.
CWV_EXPORT
@interface CWVCreditCardSaver : NSObject

// The card that can be saved.
@property(nonatomic, readonly) CWVCreditCard* creditCard;

// If not empty, contains legal messaging that must be displayed to the user.
// Contains |NSLinkAttributeName| to indicate links wherever applicable.
@property(nonatomic, readonly) NSArray<NSAttributedString*>* legalMessages;

- (instancetype)init NS_UNAVAILABLE;

// Saves |creditCard| to the user's Google Pay account.
//
// The following parameters can be different from the similarly named
// properties of |creditCard|, for example to correct the name or update the
// expiration to a valid date in the future.
// |cardHolderFullName| The full name of the card holder.
// |expirationMonth| The month MM of the expiration date. e.g. 08.
// |expirationYear| The year YYYY of the expiration date. e.g. 2021.
//
// |riskData| Needed for 1st party integration with the internal payments API.
// See go/risk-eng.g3doc for more details.
// |completionHandler| to be called with BOOL indicating if the card was saved
// or if it wasn't saved due to invalid card or network errors.
//
// This method should only be called once.
- (void)acceptWithCardHolderFullName:(NSString*)cardHolderFullName
                     expirationMonth:(NSString*)expirationMonth
                      expirationYear:(NSString*)expirationYear
                            riskData:(NSString*)riskData
                   completionHandler:(void (^)(BOOL))completionHandler;

// Rejects saving |creditCard|.
// This method should only be called once.
- (void)decline;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_SAVER_H_
