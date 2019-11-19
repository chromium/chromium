// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_SAVER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_SAVER_INTERNAL_H_

#import "ios/web_view/public/cwv_credit_card_saver.h"

#include <memory>

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"

namespace autofill {
class CreditCard;
}  // namespace autofill

@interface CWVCreditCardSaver ()

// Designated Initializer.
// |creditCard| The card that needs to be saved.
// |saveOptions| Additional options that may apply to this save attempt.
// |willUploadToCloud| Whether or not the card will be saved to the cloud.
// |legalMessage| Contains messaging that must be displayed to the user.
// |uploadSavePromptCallback| A callback to run when saving to the cloud.
// |localSavePromptCallback| A callback to run when saving locally.
- (instancetype)
          initWithCreditCard:(const autofill::CreditCard&)creditCard
                 saveOptions:(autofill::AutofillClient::SaveCreditCardOptions)
                                 saveOptions
           willUploadToCloud:(BOOL)willUploadToCloud
           legalMessageLines:(autofill::LegalMessageLines)legalMessageLines
    uploadSavePromptCallback:
        (autofill::AutofillClient::UploadSaveCardPromptCallback)
            uploadSavePromptCallback
     localSavePromptCallback:
         (autofill::AutofillClient::LocalSaveCardPromptCallback)
             localSavePromptCallback NS_DESIGNATED_INITIALIZER;

// Called to notify when upload was completed.
- (void)handleCreditCardUploadCompleted:(BOOL)cardSaved;

// Use to notify CWVCreditCardSaver that it needs to obtain risk data for
// credit card upload and to pass it back in |callback|.
- (void)loadRiskData:(base::OnceCallback<void(const std::string&)>)callback;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_SAVER_INTERNAL_H_
