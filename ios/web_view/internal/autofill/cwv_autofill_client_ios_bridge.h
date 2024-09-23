// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CLIENT_IOS_BRIDGE_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CLIENT_IOS_BRIDGE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#import "components/autofill/ios/browser/autofill_client_ios_bridge.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

// WebView extension of AutofillClientIOSBridge.
@protocol CWVAutofillClientIOSBridge<AutofillClientIOSBridge>

// Bridge for AutofillClient's method |ConfirmSaveCreditCardToCloud|.
- (void)
    confirmSaveCreditCardToCloud:(const autofill::CreditCard&)creditCard
               legalMessageLines:(autofill::LegalMessageLines)legalMessageLines
           saveCreditCardOptions:
               (autofill::payments::PaymentsAutofillClient::
                    SaveCreditCardOptions)saveCreditCardOptions
                        callback:(autofill::payments::PaymentsAutofillClient::
                                      UploadSaveCardPromptCallback)callback;

// Bridge for AutofillClient's method |CreditCardUploadCompleted|.
- (void)handleCreditCardUploadCompleted:(BOOL)cardSaved;

// Bridge for AutofillClient's method |ShowUnmaskPrompt|.
- (void)showUnmaskPromptForCard:(const autofill::CreditCard&)creditCard
        cardUnmaskPromptOptions:
            (const autofill::CardUnmaskPromptOptions&)cardUnmaskPromptOptions
                       delegate:(base::WeakPtr<autofill::CardUnmaskDelegate>)
                                    delegate;

// Bridge for AutofillClient's method |onUnmaskVerificationResult|.
- (void)didReceiveUnmaskVerificationResult:
    (autofill::payments::PaymentsAutofillClient::PaymentsRpcResult)result;

// Bridge for PaymentsAutofillClient's method `LoadRiskData`.
- (void)loadRiskData:(base::OnceCallback<void(const std::string&)>)callback;

// Bridge for AutofillClient's method |ConfirmSaveAddressProfile|.
- (void)
    confirmSaveAddressProfile:(const autofill::AutofillProfile&)profile
              originalProfile:(const autofill::AutofillProfile*)originalProfile
                     callback:(autofill::AutofillClient ::
                                   AddressProfileSavePromptCallback)callback;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CLIENT_IOS_BRIDGE_H_
