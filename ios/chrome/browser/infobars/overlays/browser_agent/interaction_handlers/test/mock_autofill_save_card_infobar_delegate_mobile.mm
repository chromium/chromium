// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_card_infobar_delegate_mobile.h"

#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/payments/autofill_save_card_delegate.h"
#import "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/signin/public/identity_manager/account_info.h"

MockAutofillSaveCardInfoBarDelegateMobile::
    MockAutofillSaveCardInfoBarDelegateMobile(
        autofill::AutofillClient::SaveCreditCardOptions options,
        const autofill::CreditCard& card,
        absl::variant<autofill::AutofillClient::LocalSaveCardPromptCallback,
                      autofill::AutofillClient::UploadSaveCardPromptCallback>
            callback,
        const autofill::LegalMessageLines& legal_message_lines,
        const AccountInfo& displayed_target_account)
    : AutofillSaveCardInfoBarDelegateMobile(
          absl::holds_alternative<
              autofill::AutofillClient::UploadSaveCardPromptCallback>(callback)
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
  using Variant =
      absl::variant<autofill::AutofillClient::LocalSaveCardPromptCallback,
                    autofill::AutofillClient::UploadSaveCardPromptCallback>;
  autofill::AutofillClient::UploadSaveCardPromptCallback upload_cb =
      base::DoNothing();
  autofill::AutofillClient::LocalSaveCardPromptCallback local_cb =
      base::DoNothing();
  return std::make_unique<MockAutofillSaveCardInfoBarDelegateMobile>(
      autofill::AutofillClient::SaveCreditCardOptions(), card,
      upload ? Variant(std::move(upload_cb)) : Variant(std::move(local_cb)),
      autofill::LegalMessageLines(
          {autofill::TestLegalMessageLine("Test message")}),
      AccountInfo());
}
