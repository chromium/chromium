// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_IOS_WEB_VIEW_PAYMENTS_AUTOFILL_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_IOS_WEB_VIEW_PAYMENTS_AUTOFILL_CLIENT_H_

#include <optional>

#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "ios/web_view/internal/autofill/cwv_autofill_client_ios_bridge.h"

class GURL;

namespace web {
class WebState;
}  // namespace web

namespace autofill {

class CreditCardCvcAuthenticator;
class WebViewAutofillClientIOS;

namespace payments {

// iOS WebView implementation of PaymentsAutofillClient. Owned by the
// WebViewAutofillClientIOS. Created lazily in the WebViewAutofillClientIOS when
// it is needed.
class IOSWebViewPaymentsAutofillClient : public PaymentsAutofillClient {
 public:
  explicit IOSWebViewPaymentsAutofillClient(
      autofill::WebViewAutofillClientIOS* client,
      id<CWVAutofillClientIOSBridge> bridge,
      web::WebState* web_state);
  IOSWebViewPaymentsAutofillClient(const IOSWebViewPaymentsAutofillClient&) =
      delete;
  IOSWebViewPaymentsAutofillClient& operator=(
      const IOSWebViewPaymentsAutofillClient&) = delete;
  ~IOSWebViewPaymentsAutofillClient() override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

  // PaymentsAutofillClient:
  void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      SaveCreditCardOptions options,
      UploadSaveCardPromptCallback callback) override;
  void CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      std::optional<OnConfirmationClosedCallback>
          on_confirmation_closed_callback) override;
  PaymentsNetworkInterface* GetPaymentsNetworkInterface() override;
  void ShowUnmaskPrompt(
      const CreditCard& card,
      const CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(
      payments::PaymentsAutofillClient::PaymentsRpcResult result) override;
  CreditCardCvcAuthenticator& GetCvcAuthenticator() override;
  void OpenPromoCodeOfferDetailsURL(const GURL& url) override;

  void set_bridge(id<CWVAutofillClientIOSBridge> bridge);

 private:
  const raw_ref<autofill::WebViewAutofillClientIOS> client_;

  __weak id<CWVAutofillClientIOSBridge> bridge_;

  std::unique_ptr<PaymentsNetworkInterface> payments_network_interface_;

  std::unique_ptr<CreditCardCvcAuthenticator> cvc_authenticator_;

  const raw_ref<web::WebState> web_state_;
};

}  // namespace payments

}  // namespace autofill

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_IOS_WEB_VIEW_PAYMENTS_AUTOFILL_CLIENT_H_
