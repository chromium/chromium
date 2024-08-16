// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_SAVER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_SAVER_INTERNAL_H_

#include <memory>

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "ios/web_view/public/cwv_credit_card_saver.h"

namespace autofill {
class CreditCard;
}  // namespace autofill

@interface CWVCreditCardSaver ()

// Designated Initializer.
// |creditCard| The card that needs to be saved.
// |saveOptions| Additional options that may apply to this save attempt.
// |legalMessage| Contains messaging that must be displayed to the user.
// |savePromptCallback| The callback to run when saving the card.
- (instancetype)initWithCreditCard:(const autofill::CreditCard&)creditCard
                       saveOptions:(autofill::payments::PaymentsAutofillClient::
                                        SaveCreditCardOptions)saveOptions
                 legalMessageLines:
                     (autofill::LegalMessageLines)legalMessageLines
                savePromptCallback:
                    (autofill::payments::PaymentsAutofillClient::
                         UploadSaveCardPromptCallback)uploadSavePromptCallback
    NS_DESIGNATED_INITIALIZER;

// Called to notify when upload was completed.
- (void)handleCreditCardUploadCompleted:(BOOL)cardSaved;

// Use to notify CWVCreditCardSaver that it needs to obtain risk data for
// credit card upload and to pass it back in |callback|.
- (void)loadRiskData:(base::OnceCallback<void(const std::string&)>)callback;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_SAVER_INTERNAL_H_
