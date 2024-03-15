// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_IOS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_IOS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_

#import "components/autofill/core/browser/payments/payments_autofill_client.h"

#import <memory>

#import "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"

class ChromeBrowserState;

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
      autofill::ChromeAutofillClientIOS* client,
      ChromeBrowserState* browser_state);
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
  void ShowAutofillErrorDialog(
      AutofillErrorDialogContext error_context) override;
  PaymentsNetworkInterface* GetPaymentsNetworkInterface() override;

  std::unique_ptr<AutofillProgressDialogControllerImpl>
  GetProgressDialogModel() {
    return std::move(progress_dialog_controller_);
  }

 private:
  const raw_ref<autofill::ChromeAutofillClientIOS> client_;

  std::unique_ptr<PaymentsNetworkInterface> payments_network_interface_;

  std::unique_ptr<AutofillProgressDialogControllerImpl>
      progress_dialog_controller_;
};

}  // namespace payments

}  // namespace autofill

#endif  //  IOS_CHROME_BROWSER_UI_AUTOFILL_IOS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_
