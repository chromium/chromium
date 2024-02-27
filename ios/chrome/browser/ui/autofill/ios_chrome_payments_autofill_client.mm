// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/ios_chrome_payments_autofill_client.h"

#import "base/check_deref.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#import "ios/chrome/browser/shared/public/commands/autofill_bottom_sheet_commands.h"
#import "ios/chrome/browser/ui/autofill/chrome_autofill_client_ios.h"
#import "ios/public/provider/chrome/browser/risk_data/risk_data_api.h"

namespace autofill::payments {

IOSChromePaymentsAutofillClient::IOSChromePaymentsAutofillClient(
    autofill::ChromeAutofillClientIOS* client)
    : client_(CHECK_DEREF(client)) {}

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

}  // namespace autofill::payments
