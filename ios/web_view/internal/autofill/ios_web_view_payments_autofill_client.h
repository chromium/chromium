// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_IOS_WEB_VIEW_PAYMENTS_AUTOFILL_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_IOS_WEB_VIEW_PAYMENTS_AUTOFILL_CLIENT_H_

#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "ios/web_view/internal/autofill/cwv_autofill_client_ios_bridge.h"

namespace autofill::payments {

// iOS WebView implementation of PaymentsAutofillClient. Owned by the
// WebViewAutofillClientIOS. Created lazily in the WebViewAutofillClientIOS when
// it is needed.
class IOSWebViewPaymentsAutofillClient : public PaymentsAutofillClient {
 public:
  explicit IOSWebViewPaymentsAutofillClient(
      id<CWVAutofillClientIOSBridge> bridge);
  IOSWebViewPaymentsAutofillClient(const IOSWebViewPaymentsAutofillClient&) =
      delete;
  IOSWebViewPaymentsAutofillClient& operator=(
      const IOSWebViewPaymentsAutofillClient&) = delete;
  ~IOSWebViewPaymentsAutofillClient() override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

  void set_bridge(id<CWVAutofillClientIOSBridge> bridge);

 private:
  __weak id<CWVAutofillClientIOSBridge> bridge_;
};

}  // namespace autofill::payments

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_IOS_WEB_VIEW_PAYMENTS_AUTOFILL_CLIENT_H_
