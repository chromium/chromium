// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_IOS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_IOS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_

#include "components/autofill/core/browser/payments/payments_autofill_client.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace autofill {

class ChromeAutofillClientIOS;
struct AutofillErrorDialogContext;

namespace payments {

// Chrome iOS implementation of PaymentsAutofillClient. Owned by the
// ChromeAutofillClientIOS. Created lazily in the ChromeAutofillClientIOS when
// it is needed.
class IOSChromePaymentsAutofillClient : public PaymentsAutofillClient {
 public:
  explicit IOSChromePaymentsAutofillClient(
      autofill::ChromeAutofillClientIOS* client);
  IOSChromePaymentsAutofillClient(const IOSChromePaymentsAutofillClient&) =
      delete;
  IOSChromePaymentsAutofillClient& operator=(
      const IOSChromePaymentsAutofillClient&) = delete;
  ~IOSChromePaymentsAutofillClient() override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

  // PaymentsAutofillClient:
  void CreditCardUploadCompleted(bool card_saved) override;

  void ShowAutofillErrorDialog(AutofillErrorDialogContext error_context);

 private:
  const raw_ref<autofill::ChromeAutofillClientIOS> client_;
};

}  // namespace payments

}  // namespace autofill

#endif  //  IOS_CHROME_BROWSER_UI_AUTOFILL_IOS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_
