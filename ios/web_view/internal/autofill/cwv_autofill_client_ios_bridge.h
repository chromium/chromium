// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CLIENT_IOS_BRIDGE_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CLIENT_IOS_BRIDGE_H_

#import "components/autofill/ios/browser/autofill_client_ios_bridge.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/card_unmask_delegate.h"

namespace autofill {
class CreditCard;
}  // namespace autofill

// WebView extension of AutofillClientIOSBridge.
@protocol CWVAutofillClientIOSBridge<AutofillClientIOSBridge>

// Bridge for AutofillClient's method |ConfirmSaveCreditCardLocally|.
- (void)confirmSaveCreditCardLocally:(const autofill::CreditCard&)creditCard
                            callback:(base::OnceClosure)callback;

// Bridge for AutofillClient's method |ShowUnmaskPrompt|.
- (void)
showUnmaskPromptForCard:(const autofill::CreditCard&)creditCard
                 reason:(autofill::AutofillClient::UnmaskCardReason)reason
               delegate:(base::WeakPtr<autofill::CardUnmaskDelegate>)delegate;

// Bridge for AutofillClient's method |onUnmaskVerificationResult|.
- (void)didReceiveUnmaskVerificationResult:
    (autofill::AutofillClient::PaymentsRpcResult)result;

// Bridge for AutofillClient's method |LoadRiskData|.
- (void)loadRiskData:(base::OnceCallback<void(const std::string&)>)callback;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CLIENT_IOS_BRIDGE_H_
