// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_IOS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_IOS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_

#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill::payments {

// Chrome iOS implementation of PaymentsAutofillClient. Owned by the
// ChromeAutofillClientIOS. Created lazily in the ChromeAutofillClientIOS when
// it is needed.
class IOSChromePaymentsAutofillClient : public PaymentsAutofillClient {
 public:
  IOSChromePaymentsAutofillClient();
  IOSChromePaymentsAutofillClient(const IOSChromePaymentsAutofillClient&) =
      delete;
  IOSChromePaymentsAutofillClient& operator=(
      const IOSChromePaymentsAutofillClient&) = delete;
  ~IOSChromePaymentsAutofillClient() override;
};

}  // namespace autofill::payments

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_IOS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_
