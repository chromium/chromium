// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_VERIFIER_DATA_SOURCE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_VERIFIER_DATA_SOURCE_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class CWVCreditCardVerifier;

// Data source for CWVCreditCardVerifer.
@protocol CWVCreditCardVerifierDataSource<NSObject>

// Called when CWVCreditCardVerifier needs risk data before it can continue with
// credit card verification. The client must obtain and return the risk data
// needed for 1st party integration with the internal payments API.
// See go/risk-eng.g3doc for more details.
- (void)creditCardVerifier:(CWVCreditCardVerifier*)creditCardVerifier
    getRiskDataWithCompletionHandler:
        (void (^)(NSString* riskData))completionHandler;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_VERIFIER_DATA_SOURCE_H_
