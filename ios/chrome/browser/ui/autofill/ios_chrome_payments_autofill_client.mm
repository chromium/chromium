// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/ios_chrome_payments_autofill_client.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/public/provider/chrome/browser/risk_data/risk_data_api.h"

namespace autofill::payments {

IOSChromePaymentsAutofillClient::IOSChromePaymentsAutofillClient() = default;

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

}  // namespace autofill::payments
