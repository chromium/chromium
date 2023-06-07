// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CLIENT_IOS_BRIDGE_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CLIENT_IOS_BRIDGE_H_

#import "components/autofill/ios/browser/autofill_client_ios_bridge.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
class FormStructure;
}  // namespace autofill

namespace web {
class WebFrame;
}  // namespace web

// WebView extension of AutofillClientIOSBridge.
@protocol CWVAutofillClientIOSBridge<AutofillClientIOSBridge>

// Bridge for AutofillClient's method |ConfirmSaveCreditCardToCloud|.
- (void)confirmSaveCreditCardToCloud:(const autofill::CreditCard&)creditCard
                   legalMessageLines:
                       (autofill::LegalMessageLines)legalMessageLines
               saveCreditCardOptions:
                   (autofill::AutofillClient::SaveCreditCardOptions)
                       saveCreditCardOptions
                            callback:(autofill::AutofillClient::
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
    (autofill::AutofillClient::PaymentsRpcResult)result;

// Bridge for AutofillClient's method |LoadRiskData|.
- (void)loadRiskData:(base::OnceCallback<void(const std::string&)>)callback;

// Bridge for AutofillClient's method |PropagateAutofillPredictions|.
- (void)propagateAutofillPredictionsForForms:
            (const std::vector<autofill::FormStructure*>&)forms
                                     inFrame:(web::WebFrame*)frame;

// Bridge for AutofillClient's method |ConfirmSaveAddressProfile|.
- (void)
    confirmSaveAddressProfile:(const autofill::AutofillProfile&)profile
              originalProfile:(const autofill::AutofillProfile*)originalProfile
                     callback:(autofill::AutofillClient ::
                                   AddressProfileSavePromptCallback)callback;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CLIENT_IOS_BRIDGE_H_
