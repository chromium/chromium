// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/ios_web_view_payments_autofill_client.h"

namespace autofill::payments {

IOSWebViewPaymentsAutofillClient::IOSWebViewPaymentsAutofillClient(
    id<CWVAutofillClientIOSBridge> bridge) {
  bridge_ = bridge;
}

IOSWebViewPaymentsAutofillClient::~IOSWebViewPaymentsAutofillClient() = default;

void IOSWebViewPaymentsAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  [bridge_ loadRiskData:std::move(callback)];
}

void IOSWebViewPaymentsAutofillClient::set_bridge(
    id<CWVAutofillClientIOSBridge> bridge) {
  bridge_ = bridge;
}

}  // namespace autofill::payments
