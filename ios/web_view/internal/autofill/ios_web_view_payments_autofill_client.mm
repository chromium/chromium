// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/ios_web_view_payments_autofill_client.h"

#import "base/check_deref.h"
#import "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#import "components/autofill/core/browser/payments/payments_network_interface.h"
#import "ios/web/public/web_state.h"
#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace autofill::payments {

IOSWebViewPaymentsAutofillClient::IOSWebViewPaymentsAutofillClient(
    autofill::WebViewAutofillClientIOS* client,
    id<CWVAutofillClientIOSBridge> bridge,
    web::BrowserState* browser_state)
    : client_(CHECK_DEREF(client)),
      bridge_(bridge),
      payments_network_interface_(
          std::make_unique<payments::PaymentsNetworkInterface>(
              base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                  browser_state->GetURLLoaderFactory()),
              client->GetIdentityManager(),
              &client->GetPersonalDataManager()->payments_data_manager(),
              browser_state->IsOffTheRecord())) {}

IOSWebViewPaymentsAutofillClient::~IOSWebViewPaymentsAutofillClient() = default;

void IOSWebViewPaymentsAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  [bridge_ loadRiskData:std::move(callback)];
}

void IOSWebViewPaymentsAutofillClient::CreditCardUploadCompleted(
    bool card_saved) {
  [bridge_ handleCreditCardUploadCompleted:card_saved];
}

payments::PaymentsNetworkInterface*
IOSWebViewPaymentsAutofillClient::GetPaymentsNetworkInterface() {
  return payments_network_interface_.get();
}

void IOSWebViewPaymentsAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    const CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  [bridge_ showUnmaskPromptForCard:card
           cardUnmaskPromptOptions:card_unmask_prompt_options
                          delegate:delegate];
}

void IOSWebViewPaymentsAutofillClient::OnUnmaskVerificationResult(
    AutofillClient::PaymentsRpcResult result) {
  [bridge_ didReceiveUnmaskVerificationResult:result];
}

CreditCardCvcAuthenticator&
IOSWebViewPaymentsAutofillClient::GetCvcAuthenticator() {
  if (!cvc_authenticator_) {
    cvc_authenticator_ =
        std::make_unique<CreditCardCvcAuthenticator>(&client_.get());
  }
  return *cvc_authenticator_;
}

void IOSWebViewPaymentsAutofillClient::set_bridge(
    id<CWVAutofillClientIOSBridge> bridge) {
  bridge_ = bridge;
}

}  // namespace autofill::payments
