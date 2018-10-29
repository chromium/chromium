// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_VERIFIER_DELEGATE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_VERIFIER_DELEGATE_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class CWVCreditCardVerifier;

// Delegate of CWVCreditCardVerifier.
@protocol CWVCreditCardVerifierDelegate<NSObject>

@optional

// Called when CWVCreditCardVerifier could not verify the credit card.
// |error| nil if successful, non-nil if unsuccessful. User info will contain
// key CWVCreditCardVerifierErrorMessageKey indicating the reason and
// CWVCreditCardVerifierRetryAllowedKey indicating if user can try again.
- (void)creditCardVerifier:(CWVCreditCardVerifier*)creditCardVerifier
    didFinishVerificationWithError:(nullable NSError*)error;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_VERIFIER_DELEGATE_H_
