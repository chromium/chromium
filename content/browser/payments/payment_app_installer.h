// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_INSTALLER_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_INSTALLER_H_

#include <stdint.h>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/browser/supported_delegations.h"

class GURL;

namespace content {

class BrowserContext;
class WebContents;

// Installs a web payment app with a default payment instrument and returns
// the registration Id through callback on success.
class PaymentAppInstaller {
 public:
  using InstallPaymentAppCallback =
      base::OnceCallback<void(BrowserContext* browser_context,
                              int64_t registration_id)>;

  // Installs the payment app.
  // |app_name| is the name of the payment app.
  // |app_icon| is the icon of the payment app.
  // |sw_url| is the url to get the service worker js script.
  // |scope| is the registration scope.
  // |use_cache| indicates whether to use cache.
  // |enabled_methods| are the enabled methods of the app.
  // |supported_delegations| are the supported delegations of the payment app.
  // |callback| to send back registration result.
  static void Install(WebContents* web_contents,
                      const std::string& app_name,
                      const std::string& app_icon,
                      const GURL& sw_url,
                      const GURL& scope,
                      bool use_cache,
                      const std::string& method,
                      const SupportedDelegations& supported_delegations,
                      InstallPaymentAppCallback callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PaymentAppInstaller);
};

}  // namespace content.

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_INSTALLER_H_
