// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_

#include <memory>
#include <string>

#include "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;

class MockAutofillSaveCardInfoBarDelegateMobile
    : public autofill::AutofillSaveCardInfoBarDelegateMobile {
 public:
  MockAutofillSaveCardInfoBarDelegateMobile(
      bool upload,
      autofill::AutofillClient::SaveCreditCardOptions options,
      const autofill::CreditCard& card,
      const autofill::LegalMessageLines& legal_message_lines,
      autofill::AutofillClient::UploadSaveCardPromptCallback
          upload_save_card_prompt_callback,
      autofill::AutofillClient::LocalSaveCardPromptCallback
          local_save_card_prompt_callback,
      const AccountInfo& displayed_target_account);
  ~MockAutofillSaveCardInfoBarDelegateMobile() override;

  MOCK_METHOD3(UpdateAndAccept,
               bool(std::u16string cardholder_name,
                    std::u16string expiration_date_month,
                    std::u16string expiration_date_year));
  MOCK_METHOD1(OnLegalMessageLinkClicked, void(GURL url));
  MOCK_METHOD0(InfoBarDismissed, void());
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

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_
