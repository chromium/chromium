// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PAYMENT_APP_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_PAYMENT_APP_PROVIDER_H_

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"

class SkBitmap;

namespace url {
class Origin;
}  // namespace url

namespace content {

class WebContents;
struct SupportedDelegations;

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
  static PaymentAppProvider* GetOrCreateForWebContents(
      WebContents* payment_request_web_contents);

  using RegistrationIdCallback =
      base::OnceCallback<void(int64_t registration_id)>;
  using InvokePaymentAppCallback =
      base::OnceCallback<void(payments::mojom::PaymentHandlerResponsePtr)>;
  using CanMakePaymentCallback =
      base::OnceCallback<void(payments::mojom::CanMakePaymentResponsePtr)>;
  using AbortCallback = base::OnceCallback<void(bool)>;
  using UpdatePaymentAppIconCallback =
      base::OnceCallback<void(payments::mojom::PaymentHandlerStatus status)>;

  // Should be accessed only on the UI thread.
  virtual void InvokePaymentApp(
      int64_t registration_id,
      const url::Origin& sw_origin,
      payments::mojom::PaymentRequestEventDataPtr event_data,
      InvokePaymentAppCallback callback) = 0;
  virtual void InstallAndInvokePaymentApp(
      payments::mojom::PaymentRequestEventDataPtr event_data,
      const std::string& app_name,
      const SkBitmap& app_icon,
      const GURL& sw_js_url,
      const GURL& sw_scope,
      bool sw_use_cache,
      const std::string& method,
      const SupportedDelegations& supported_delegations,
      RegistrationIdCallback registration_id_callback,
      InvokePaymentAppCallback callback) = 0;
  virtual void UpdatePaymentAppIcon(
      int64_t registration_id,
      const std::string& instrument_key,
      const std::string& name,
      const std::string& string_encoded_icon,
      const std::string& method_name,
      const SupportedDelegations& supported_delegations,
      UpdatePaymentAppIconCallback callback) = 0;
  virtual void CanMakePayment(
      int64_t registration_id,
      const url::Origin& sw_origin,
      const std::string& payment_request_id,
      payments::mojom::CanMakePaymentEventDataPtr event_data,
      CanMakePaymentCallback callback) = 0;
  virtual void AbortPayment(int64_t registration_id,
                            const url::Origin& sw_origin,
                            const std::string& payment_request_id,
                            AbortCallback callback) = 0;

  // Set opened window for payment handler. Note that we maintain at most one
  // opened window for payment handler at any moment in a browser context. The
  // previously opened window in the same browser context will be closed after
  // calling this interface.
  virtual void SetOpenedWindow(WebContents* payment_handler_web_contents) = 0;
  virtual void CloseOpenedWindow() = 0;

  // Notify the opened payment handler window is closing or closed by user so as
  // to abort payment request.
  virtual void OnClosingOpenedWindow(
      payments::mojom::PaymentEventResponseType reason) = 0;

  // A test-only method for installing a service worker based payment app.
  // Invokes the `callback` when done.
  virtual void InstallPaymentAppForTesting(
      const SkBitmap& app_icon,
      const GURL& service_worker_javascript_file_url,
      const GURL& service_worker_scope,
      const std::string& payment_method_identifier,
      base::OnceCallback<void(bool success)> callback) = 0;

 protected:
  virtual ~PaymentAppProvider() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PAYMENT_APP_PROVIDER_H_
