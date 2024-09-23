// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_VERIFIER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_VERIFIER_INTERNAL_H_

#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "ios/web_view/public/cwv_credit_card_verifier.h"

NS_ASSUME_NONNULL_BEGIN

namespace autofill {
class CardUnmaskDelegate;
class CreditCard;
}  // namespace autofill

@interface CWVCreditCardVerifier ()

// Designated initializer.
// |prefs| The associated pref service. Must outlive this instance.
// |isOffTheRecord| The associated browser state's off the record state.
// |creditCard| The card that needs verification.
// |reason| Why the card needs verification.
// |delegate| Internal delegate used process verification events.
- (instancetype)
     initWithPrefs:(PrefService*)prefs
    isOffTheRecord:(BOOL)isOffTheRecord
        creditCard:(const autofill::CreditCard&)creditCard
            reason:
                (autofill::payments::PaymentsAutofillClient::UnmaskCardReason)
                    reason
          delegate:(base::WeakPtr<autofill::CardUnmaskDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

// Used to pass unmask verification results to this class. Needed because
// verification results arrive from the CWVAutofillClientBridge protocol defiend
// in CWVAutofillController.
- (void)didReceiveUnmaskVerificationResult:
    (autofill::payments::PaymentsAutofillClient::PaymentsRpcResult)result;

// Use to notify CWVCreditCardVerifier that it needs to obtain risk data for
// credit card verification and to pass it back in |callback|.
- (void)loadRiskData:(base::OnceCallback<void(const std::string&)>)callback;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_VERIFIER_INTERNAL_H_
