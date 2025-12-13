// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_OTP_VERIFIER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_OTP_VERIFIER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVCreditCard;
@class CWVCreditCardOTPVerifier;

// The error domain for OTP verification errors.
FOUNDATION_EXPORT CWV_EXPORT
    NSErrorDomain const CWVCreditCardOTPVerifierErrorDomain;

// Possible error codes for CWVCreditCardOTPVerifierErrorDomain.
typedef NS_ENUM(NSInteger, CWVCreditCardOTPVerificationError) {
  // Unknown error.
  CWVCreditCardOTPVerificationErrorUnknown = 0,
  // The OTP entered did not match.
  CWVCreditCardOTPVerificationErrorMismatch = -101,
  // The OTP entered has expired.
  CWVCreditCardOTPVerificationErrorExpired = -102,
  // A permanent failure occurred, e.g. too many attempts.
  CWVCreditCardOTPVerificationErrorPermanentFailure = -103,
  // The user cancelled the operation.
  CWVCreditCardOTPVerificationErrorUserCancelled = -104,
};

// Delegate protocol for CWVCreditCardOTPVerifier to communicate UI updates.
@protocol CWVCreditCardOTPVerifierDelegate <NSObject>

// Called when an error message should be displayed to the user (e.g., invalid
// OTP).
- (void)creditCardOTPVerifier:(CWVCreditCardOTPVerifier*)verifier
          displayErrorMessage:(NSString*)message;

// Called when the verifier enters a pending state (e.g., waiting for server
// response).
- (void)creditCardOTPVerifierShowPendingState:
    (CWVCreditCardOTPVerifier*)verifier;

// Called when the verifier exits the pending state.
- (void)creditCardOTPVerifierHidePendingState:
    (CWVCreditCardOTPVerifier*)verifier;

@end

// Manages the UI and logic for verifying a credit card unmasking with an OTP
// (One-Time Password).
CWV_EXPORT
@interface CWVCreditCardOTPVerifier : NSObject

// The credit card that is pending unmasking.
@property(nonatomic, readonly) CWVCreditCard* creditCard;

// A recommended title to display to the user.
@property(nonatomic, readonly) NSString* dialogTitle;

// The placeholder text for the OTP input field.
@property(nonatomic, readonly) NSString* textfieldPlaceholderText;

// A recommended button label for the confirm/OK button.
@property(nonatomic, readonly) NSString* okButtonLabel;

// The delegate to handle UI updates.
@property(nonatomic, weak) id<CWVCreditCardOTPVerifierDelegate> delegate;

// The text for the link to request a new OTP code.
@property(nonatomic, readonly) NSString* newCodeLinkText;

- (instancetype)init NS_UNAVAILABLE;

// Checks if the entered |OTP| string is potentially valid based on length and
// character set (digits only).
- (BOOL)isOTPValid:(NSString*)OTP;

// Attempts to verify the unmasking with the provided |OTP|.
// |OTP| The One-Time Password entered by the user.
// |completionHandler| will be called upon completion. |error| is nil if the
// verification succeeded. If non-nil, the error's code will be one of
// |CWVCreditCardOTPVerificationError|, and the |userInfo| dictionary may
// contain |NSLocalizedDescriptionKey| with a user-facing message.
- (void)verifyWithOTP:(NSString*)OTP
    completionHandler:(void (^)(NSError* _Nullable error))completionHandler;

// Call this method to indicate the user has requested a new OTP. This will
// typically involve an action on the underlying delegate.
- (void)requestNewOTP;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_OTP_VERIFIER_H_
