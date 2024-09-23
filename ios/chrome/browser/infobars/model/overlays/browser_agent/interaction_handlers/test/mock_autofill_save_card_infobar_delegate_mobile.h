// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_

#include <memory>
#include <string>

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "ios/chrome/browser/autofill/model/credit_card/autofill_save_card_infobar_delegate_ios.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;

class MockAutofillSaveCardInfoBarDelegateMobile
    : public autofill::AutofillSaveCardInfoBarDelegateIOS {
 public:
  MockAutofillSaveCardInfoBarDelegateMobile(
      autofill::payments::PaymentsAutofillClient::SaveCreditCardOptions options,
      const autofill::CreditCard& card,
      absl::variant<autofill::payments::PaymentsAutofillClient::
                        LocalSaveCardPromptCallback,
                    autofill::payments::PaymentsAutofillClient::
                        UploadSaveCardPromptCallback> callback,
      const autofill::LegalMessageLines& legal_message_lines,
      const AccountInfo& displayed_target_account);
  ~MockAutofillSaveCardInfoBarDelegateMobile() override;

  MOCK_METHOD(bool,
              UpdateAndAccept,
              (std::u16string cardholder_name,
               std::u16string expiration_date_month,
               std::u16string expiration_date_year),
              (override));
  MOCK_METHOD(void, OnLegalMessageLinkClicked, (GURL url), (override));
  MOCK_METHOD(void, InfoBarDismissed, (), (override));
  MOCK_METHOD(void,
              CreditCardUploadCompleted,
              (bool card_saved,
               std::optional<autofill::payments::PaymentsAutofillClient::
                                 OnConfirmationClosedCallback>
                   on_confirmation_closed_callback),
              (override));
  MOCK_METHOD(void, OnConfirmationClosed, (), (override));
  MOCK_METHOD(bool, IsCreditCardUploadComplete, (), (override));
  MOCK_METHOD(void,
              SetCreditCardUploadCompletionCallback,
              (base::OnceCallback<void(bool card_saved)>
                   credit_card_upload_completion_callback),
              (override));
  MOCK_METHOD(void, SetInfobarIsPresenting, (bool is_presenting), (override));
};

class MockAutofillSaveCardInfoBarDelegateMobileFactory {
 public:
  MockAutofillSaveCardInfoBarDelegateMobileFactory();
  ~MockAutofillSaveCardInfoBarDelegateMobileFactory();

  static std::unique_ptr<MockAutofillSaveCardInfoBarDelegateMobile>
  CreateMockAutofillSaveCardInfoBarDelegateMobileFactory(
      bool upload,
      autofill::CreditCard card);

 private:
  autofill::CreditCard credit_card_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_
