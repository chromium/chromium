// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_card_infobar_delegate_mobile.h"

#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/payments/autofill_save_card_delegate.h"
#import "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/signin/public/identity_manager/account_info.h"

MockAutofillSaveCardInfoBarDelegateMobile::
    MockAutofillSaveCardInfoBarDelegateMobile(
        autofill::payments::PaymentsAutofillClient::SaveCreditCardOptions
            options,
        const autofill::CreditCard& card,
        absl::variant<autofill::payments::PaymentsAutofillClient::
                          LocalSaveCardPromptCallback,
                      autofill::payments::PaymentsAutofillClient::
                          UploadSaveCardPromptCallback> callback,
        const autofill::LegalMessageLines& legal_message_lines,
        const AccountInfo& displayed_target_account)
    : AutofillSaveCardInfoBarDelegateIOS(
          absl::holds_alternative<autofill::payments::PaymentsAutofillClient::
                                      UploadSaveCardPromptCallback>(callback)
              ? autofill::AutofillSaveCardUiInfo::CreateForUploadSave(
                    options,
                    card,
                    legal_message_lines,
                    displayed_target_account)
              : autofill::AutofillSaveCardUiInfo::CreateForLocalSave(options,
                                                                     card),
          std::make_unique<autofill::AutofillSaveCardDelegate>(
              std::move(callback),
              options)) {}

MockAutofillSaveCardInfoBarDelegateMobile::
    ~MockAutofillSaveCardInfoBarDelegateMobile() = default;

#pragma mark - MockAutofillSaveCardInfoBarDelegateMobileFactory

MockAutofillSaveCardInfoBarDelegateMobileFactory::
    MockAutofillSaveCardInfoBarDelegateMobileFactory()
    : credit_card_(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                   "https://www.example.com/") {}

MockAutofillSaveCardInfoBarDelegateMobileFactory::
    ~MockAutofillSaveCardInfoBarDelegateMobileFactory() {}

std::unique_ptr<MockAutofillSaveCardInfoBarDelegateMobile>
MockAutofillSaveCardInfoBarDelegateMobileFactory::
    CreateMockAutofillSaveCardInfoBarDelegateMobileFactory(
        bool upload,
        autofill::CreditCard card) {
  using Variant = absl::variant<
      autofill::payments::PaymentsAutofillClient::LocalSaveCardPromptCallback,
      autofill::payments::PaymentsAutofillClient::UploadSaveCardPromptCallback>;
  autofill::payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
      upload_cb = base::DoNothing();
  autofill::payments::PaymentsAutofillClient::LocalSaveCardPromptCallback
      local_cb = base::DoNothing();
  return std::make_unique<MockAutofillSaveCardInfoBarDelegateMobile>(
      autofill::payments::PaymentsAutofillClient::SaveCreditCardOptions(), card,
      upload ? Variant(std::move(upload_cb)) : Variant(std::move(local_cb)),
      autofill::LegalMessageLines(
          {autofill::TestLegalMessageLine("Test message")}),
      AccountInfo());
}
