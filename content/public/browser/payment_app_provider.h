// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PAYMENT_APP_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_PAYMENT_APP_PROVIDER_H_

#include <stdint.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/stored_payment_app.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"

class SkBitmap;

namespace url {
class Origin;
}  // namespace url

namespace content {

class BrowserContext;
class WebContents;

// This is providing the service worker based payment app related APIs to
// Chrome layer. This class is a singleton, the instance of which can be
// retrieved using the static GetInstance() method.
// All methods must be called on the UI thread.
//
// Design Doc:
//   https://docs.google.com/document/d/1rWsvKQAwIboN2ZDuYYAkfce8GF27twi4UHTt0hcbyxQ/edit?usp=sharing
class CONTENT_EXPORT PaymentAppProvider {
 public:
  // This static function is actually implemented in PaymentAppProviderImpl.cc.
  // Please see: content/browser/payments/payment_app_provider_impl.cc
  static PaymentAppProvider* GetInstance();

  using PaymentApps = std::map<int64_t, std::unique_ptr<StoredPaymentApp>>;
  using GetAllPaymentAppsCallback = base::OnceCallback<void(PaymentApps)>;
  using RegistrationIdCallback =
      base::OnceCallback<void(int64_t registration_id)>;
  using InvokePaymentAppCallback =
      base::OnceCallback<void(payments::mojom::PaymentHandlerResponsePtr)>;
  using PaymentEventResultCallback = base::OnceCallback<void(bool)>;

  // Should be accessed only on the UI thread.
  virtual void GetAllPaymentApps(BrowserContext* browser_context,
                                 GetAllPaymentAppsCallback callback) = 0;
  virtual void InvokePaymentApp(
      BrowserContext* browser_context,
      int64_t registration_id,
      const url::Origin& sw_origin,
      payments::mojom::PaymentRequestEventDataPtr event_data,
      InvokePaymentAppCallback callback) = 0;
  virtual void InstallAndInvokePaymentApp(
      WebContents* web_contents,
      payments::mojom::PaymentRequestEventDataPtr event_data,
      const std::string& app_name,
      const SkBitmap& app_icon,
      const std::string& sw_js_url,
      const std::string& sw_scope,
      bool sw_use_cache,
      const std::string& method,
      const SupportedDelegations& supported_delegations,
      RegistrationIdCallback registration_id_callback,
      InvokePaymentAppCallback callback) = 0;
  virtual void CanMakePayment(
      BrowserContext* browser_context,
      int64_t registration_id,
      const url::Origin& sw_origin,
      const std::string& payment_request_id,
      payments::mojom::CanMakePaymentEventDataPtr event_data,
      PaymentEventResultCallback callback) = 0;
  virtual void AbortPayment(BrowserContext* browser_context,
                            int64_t registration_id,
                            const url::Origin& sw_origin,
                            const std::string& payment_request_id,
                            PaymentEventResultCallback callback) = 0;

  // Set opened window for payment handler. Note that we maintain at most one
  // opened window for payment handler at any moment in a browser context. The
  // previously opened window in the same browser context will be closed after
  // calling this interface.
  virtual void SetOpenedWindow(WebContents* web_contents) = 0;
  virtual void CloseOpenedWindow(BrowserContext* browser_context) = 0;

  // Notify the opened payment handler window is closing or closed by user so as
  // to abort payment request.
  virtual void OnClosingOpenedWindow(
      BrowserContext* browser_context,
      payments::mojom::PaymentEventResponseType reason) = 0;

  // Check whether given |sw_js_url| from |manifest_url| is allowed to register
  // with |sw_scope|.
  virtual bool IsValidInstallablePaymentApp(const GURL& manifest_url,
                                            const GURL& sw_js_url,
                                            const GURL& sw_scope,
                                            std::string* error_message) = 0;

 protected:
  virtual ~PaymentAppProvider() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PAYMENT_APP_PROVIDER_H_
