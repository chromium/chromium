// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_card_infobar_delegate_mobile.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "base/bind.h"
#include "base/guid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/signin/public/identity_manager/account_info.h"

MockAutofillSaveCardInfoBarDelegateMobile::
    MockAutofillSaveCardInfoBarDelegateMobile(
        bool upload,
        autofill::AutofillClient::SaveCreditCardOptions options,
        const autofill::CreditCard& card,
        const autofill::LegalMessageLines& legal_message_lines,
        autofill::AutofillClient::UploadSaveCardPromptCallback
            upload_save_card_prompt_callback,
        autofill::AutofillClient::LocalSaveCardPromptCallback
            local_save_card_prompt_callback,
        const AccountInfo& displayed_target_account)
    : AutofillSaveCardInfoBarDelegateMobile(
          upload,
          options,
          card,
          legal_message_lines,
          std::move(upload_save_card_prompt_callback),
          std::move(local_save_card_prompt_callback),
          displayed_target_account) {}

MockAutofillSaveCardInfoBarDelegateMobile::
    ~MockAutofillSaveCardInfoBarDelegateMobile() = default;

#pragma mark - MockAutofillSaveCardInfoBarDelegateMobileFactory

MockAutofillSaveCardInfoBarDelegateMobileFactory::
    MockAutofillSaveCardInfoBarDelegateMobileFactory()
    : credit_card_(base::GenerateGUID(), "https://www.example.com/") {}

MockAutofillSaveCardInfoBarDelegateMobileFactory::
    ~MockAutofillSaveCardInfoBarDelegateMobileFactory() {}

std::unique_ptr<MockAutofillSaveCardInfoBarDelegateMobile>
MockAutofillSaveCardInfoBarDelegateMobileFactory::
    CreateMockAutofillSaveCardInfoBarDelegateMobileFactory(
        bool upload,
        autofill::CreditCard card) {
  return std::make_unique<MockAutofillSaveCardInfoBarDelegateMobile>(
      /*upload=*/upload, autofill::AutofillClient::SaveCreditCardOptions(),
      card, autofill::LegalMessageLines(),
      autofill::AutofillClient::UploadSaveCardPromptCallback(),
      base::BindOnce(
          ^(autofill::AutofillClient::SaveCardOfferUserDecision user_decision){
          }),
      AccountInfo());
}
