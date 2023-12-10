// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_IOS_WEB_VIEW_PAYMENTS_AUTOFILL_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_IOS_WEB_VIEW_PAYMENTS_AUTOFILL_CLIENT_H_

#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill::payments {

// iOS WebView implementation of PaymentsAutofillClient. Owned by the
// WebViewAutofillClientIOS. Created lazily in the WebViewAutofillClientIOS when
// it is needed.
class IOSWebViewPaymentsAutofillClient : public PaymentsAutofillClient {
 public:
  IOSWebViewPaymentsAutofillClient();
  IOSWebViewPaymentsAutofillClient(const IOSWebViewPaymentsAutofillClient&) =
      delete;
  IOSWebViewPaymentsAutofillClient& operator=(
      const IOSWebViewPaymentsAutofillClient&) = delete;
  ~IOSWebViewPaymentsAutofillClient() override;
};

}  // namespace autofill::payments

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_IOS_WEB_VIEW_PAYMENTS_AUTOFILL_CLIENT_H_
