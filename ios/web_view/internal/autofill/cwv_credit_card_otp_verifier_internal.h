// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_OTP_VERIFIER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_OTP_VERIFIER_INTERNAL_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/otp_unmask_delegate.h"
#include "components/autofill/core/browser/payments/otp_unmask_result.h"
#import "ios/web_view/public/cwv_credit_card_otp_verifier.h"

NS_ASSUME_NONNULL_BEGIN

namespace autofill {
class OtpUnmaskDelegate;
}  // namespace autofill

@interface CWVCreditCardOTPVerifier ()

// Designated initializer.
// |cardType|: The type of the credit card.
// |challengeOption|: The specific OTP challenge option details.
// |unmaskDelegate|: Internal delegate used to process OTP unmasking events.
- (instancetype)
    initWithCardType:(autofill::CreditCard::RecordType)cardType
     challengeOption:(const autofill::CardUnmaskChallengeOption&)challengeOption
      unmaskDelegate:(base::WeakPtr<autofill::OtpUnmaskDelegate>)unmaskDelegate
    NS_DESIGNATED_INITIALIZER;

// Called by the C++ view when the dialog is dismissed.
// |success|: YES if the overall OTP flow was successful.
// |userClosedDialog|: YES if the user manually closed the dialog.
- (void)dialogDidDismissWithSuccess:(BOOL)success
                   userClosedDialog:(BOOL)userClosedDialog;

// Used to pass unmask verification results to this class. Needed because
// verification results arrive from the CWVAutofillClientBridge protocol defined
// in CWVAutofillController.
- (void)didReceiveUnmaskOtpVerificationResult:(autofill::OtpUnmaskResult)result;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_OTP_VERIFIER_INTERNAL_H_
