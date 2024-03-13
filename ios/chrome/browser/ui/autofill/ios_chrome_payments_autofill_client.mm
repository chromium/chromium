// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/ios_chrome_payments_autofill_client.h"

#import "base/check_deref.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#import "components/autofill/core/browser/payments/payments_network_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/ui/autofill/chrome_autofill_client_ios.h"
#import "ios/public/provider/chrome/browser/risk_data/risk_data_api.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace autofill::payments {

IOSChromePaymentsAutofillClient::IOSChromePaymentsAutofillClient(
    autofill::ChromeAutofillClientIOS* client,
    ChromeBrowserState* browser_state)
    : client_(CHECK_DEREF(client)),
      payments_network_interface_(
          std::make_unique<payments::PaymentsNetworkInterface>(
              base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                  browser_state->GetURLLoaderFactory()),
              client->GetIdentityManager(),
              client->GetPersonalDataManager(),
              browser_state->IsOffTheRecord())) {}

IOSChromePaymentsAutofillClient::~IOSChromePaymentsAutofillClient() = default;

void IOSChromePaymentsAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  std::move(callback).Run(
      base::SysNSStringToUTF8(ios::provider::GetRiskData()));
}

void IOSChromePaymentsAutofillClient::CreditCardUploadCompleted(
    bool card_saved) {
  NOTIMPLEMENTED();
}

void IOSChromePaymentsAutofillClient::ShowAutofillErrorDialog(
    AutofillErrorDialogContext error_context) {
  [client_->commands_handler()
      showAutofillErrorDialog:std::move(error_context)];
}

PaymentsNetworkInterface*
IOSChromePaymentsAutofillClient::GetPaymentsNetworkInterface() {
  return payments_network_interface_.get();
}

}  // namespace autofill::payments
