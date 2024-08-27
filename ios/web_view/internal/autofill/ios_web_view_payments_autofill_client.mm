// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/ios_web_view_payments_autofill_client.h"

#import <optional>

#import "base/check_deref.h"
#import "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/core/browser/payments/payments_network_interface.h"
#import "ios/web/public/web_state.h"
#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "url/gurl.h"

namespace autofill::payments {

IOSWebViewPaymentsAutofillClient::IOSWebViewPaymentsAutofillClient(
    autofill::WebViewAutofillClientIOS* client,
    id<CWVAutofillClientIOSBridge> bridge,
    web::WebState* web_state)
    : client_(CHECK_DEREF(client)),
      bridge_(bridge),
      payments_network_interface_(
          std::make_unique<payments::PaymentsNetworkInterface>(
              base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                  web_state->GetBrowserState()->GetURLLoaderFactory()),
              client->GetIdentityManager(),
              &client->GetPersonalDataManager()->payments_data_manager(),
              web_state->GetBrowserState()->IsOffTheRecord())),
      web_state_(CHECK_DEREF(web_state)) {}

IOSWebViewPaymentsAutofillClient::~IOSWebViewPaymentsAutofillClient() = default;

void IOSWebViewPaymentsAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  [bridge_ loadRiskData:std::move(callback)];
}

void IOSWebViewPaymentsAutofillClient::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
  DCHECK(options.show_prompt);
  [bridge_ confirmSaveCreditCardToCloud:card
                      legalMessageLines:legal_message_lines
                  saveCreditCardOptions:options
                               callback:std::move(callback)];
}

void IOSWebViewPaymentsAutofillClient::CreditCardUploadCompleted(
    payments::PaymentsAutofillClient::PaymentsRpcResult result,
    std::optional<OnConfirmationClosedCallback>
        on_confirmation_closed_callback) {
  const bool card_saved =
      result == payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess;
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
    payments::PaymentsAutofillClient::PaymentsRpcResult result) {
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

void IOSWebViewPaymentsAutofillClient::OpenPromoCodeOfferDetailsURL(
    const GURL& url) {
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      /*is_renderer_initiated=*/false));
}

void IOSWebViewPaymentsAutofillClient::set_bridge(
    id<CWVAutofillClientIOSBridge> bridge) {
  bridge_ = bridge;
}

}  // namespace autofill::payments
