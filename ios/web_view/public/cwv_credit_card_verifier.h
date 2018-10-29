// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_VERIFIER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_VERIFIER_H_

#import <UIKit/UIKit.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVCreditCard;
@protocol CWVCreditCardVerifierDataSource;
@protocol CWVCreditCardVerifierDelegate;

// The error domain for credit card verification errors.
FOUNDATION_EXPORT CWV_EXPORT
    NSErrorDomain const CWVCreditCardVerifierErrorDomain;
// The key for the error message value in the error's |userInfo| dictionary.
FOUNDATION_EXPORT CWV_EXPORT
    NSString* const CWVCreditCardVerifierErrorMessageKey;
// The key for the retry allowed value in the error's |userInfo| dictionary.
FOUNDATION_EXPORT CWV_EXPORT
    NSString* const CWVCreditCardVerifierRetryAllowedKey;

// Possible error codes during credit card verification.
typedef NS_ENUM(NSInteger, CWVCreditCardVerificationError) {
  // No errors.
  CWVCreditCardVerificationErrorNone = 0,
  // Request failed; try again.
  CWVCreditCardVerificationErrorTryAgainFailure = -100,
  // Request failed; don't try again.
  CWVCreditCardVerificationErrorPermanentFailure = -200,
  // Unable to connect to Payments servers. Prompt user to check internet
  // connection.
  CWVCreditCardVerificationErrorNetworkFailure = -300,
};

CWV_EXPORT
// Helps with verifying credit cards for autofill, updating expired expiration
// dates, and saving the card locally.
@interface CWVCreditCardVerifier : NSObject

// The credit card that is pending verification.
@property(nonatomic, readonly) CWVCreditCard* creditCard;

// Whether or not this card can be saved locally.
@property(nonatomic, readonly) BOOL canStoreLocally;

// The last |storeLocally| value that was used when verifying. Can be used to
// set initial state for UI.
@property(nonatomic, readonly) BOOL lastStoreLocallyValue;

// Returns a recommended title to display in the navigation bar to the user.
@property(nonatomic, readonly) NSString* navigationTitle;

// Returns the instruction message to show the user for verifying |creditCard|.
// Depends on |needsUpdateForExpirationDate| and |canSaveLocally|.
@property(nonatomic, readonly) NSString* instructionMessage;

// Returns a recommended button label for a confirm/OK button.
@property(nonatomic, readonly) NSString* confirmButtonLabel;

// Returns an image that indicates where on the card you may find the CVC.
@property(nonatomic, readonly) UIImage* CVCHintImage;

// The expected length of the CVC depending on |creditCard|'s network.
// e.g. 3 for Visa and 4 for American Express.
@property(nonatomic, readonly) NSInteger expectedCVCLength;

// YES if |creditCard|'s current expiration date has expired and needs updating.
@property(nonatomic, readonly) BOOL needsUpdateForExpirationDate;

- (instancetype)init NS_UNAVAILABLE;

// Attempts |creditCard| verification.
// |CVC| Card verification code. e.g. 3 digit code on the back of Visa cards or
// 4 digit code in the front of American Express cards.
// |month| 1 or 2 digit expiration month. e.g. 8 or 08 for August. Can be nil if
// |needsUpdateForExpirationDate| is NO.
// |year| 2 or 4 digit expiration year. e.g. 19 or 2019. Can be nil if
// |needsUpdateForExpirationDate| is NO.
// |storeLocally| Whether or not to save |creditCard| locally. If YES, user will
// not be asked again to verify this card. Ignored if |canSaveLocally| is NO.
// |dataSource| will be asked to return risk data needed for verification.
// |delegate| will be passed the verification result.
// If |delegate| is passed an error object indicating retry is not allowed,
// additional verifications will be ignored.
- (void)verifyWithCVC:(NSString*)CVC
      expirationMonth:(nullable NSString*)expirationMonth
       expirationYear:(nullable NSString*)expirationYear
         storeLocally:(BOOL)storeLocally
           dataSource:(__weak id<CWVCreditCardVerifierDataSource>)dataSource
             delegate:
                 (nullable __weak id<CWVCreditCardVerifierDelegate>)delegate;

// Returns YES if |CVC| is all digits and matches |expectedCVCLength|.
- (BOOL)isCVCValid:(NSString*)CVC;

// Returns YES if |month| and |year| is in the future.
// |month| 1 or 2 digit. e.g. 8 or 08 for August.
// |year| 2 or 4 digit. e.g. 20 or 2020.
- (BOOL)isExpirationDateValidForMonth:(NSString*)month year:(NSString*)year;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_VERIFIER_H_
