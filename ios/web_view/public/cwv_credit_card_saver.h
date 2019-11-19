// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_SAVER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_SAVER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVCreditCard;

// Helps with saving a credit card locally or uploading to the cloud.
// To make a decision, there are 3 options:
// 1. Call |acceptWithRiskData:completionHandler:| to accept the save.
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

// Whether or not this card will be saved to Google Pay or local disk. i.e.,
// |YES| means the card is uploaded to Google Pay but NOT stored on local disk.
// |NO| means the card is stored on local disk and NOT uploaded to Google Pay.
// If Chrome Sync is enabled, this value will be |YES|.
@property(nonatomic, readonly) BOOL willUploadToCloud;

- (instancetype)init NS_UNAVAILABLE;

// Saves |creditCard| to device or cloud according to |willUploadToCloud|.
// |riskData| Needed for 1st party integration with the internal payments API.
// Only required if |willUploadToCloud|. See go/risk-eng.g3doc for more details.
// |completionHandler| to be called with BOOL indicating if the card was saved
// or if it wasn't saved due to invalid card or network errors.
// This method should only be called once.
- (void)acceptWithRiskData:(nullable NSString*)riskData
         completionHandler:(void (^_Nullable)(BOOL))completionHandler;

// Rejects saving |creditCard|.
// This method should only be called once.
- (void)decline;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_SAVER_H_
