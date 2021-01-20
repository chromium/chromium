// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_PROVIDER_IMPL_H_

#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/payments/service_worker_core_thread_event_dispatcher.h"
#include "content/common/content_export.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {

class CONTENT_EXPORT PaymentAppProviderImpl
    : public PaymentAppProvider,
      public WebContentsUserData<PaymentAppProviderImpl> {
 public:
  ~PaymentAppProviderImpl() override;
  static PaymentAppProviderImpl* GetOrCreateForWebContents(
      WebContents* payment_request_web_contents);

  // Disallow copy and assign.
  PaymentAppProviderImpl(const PaymentAppProviderImpl& other) = delete;
  PaymentAppProviderImpl& operator=(const PaymentAppProviderImpl& other) =
      delete;

  // PaymentAppProvider implementation:
  // Should be accessed only on the UI thread.
  void InvokePaymentApp(int64_t registration_id,
                        const url::Origin& sw_origin,
                        payments::mojom::PaymentRequestEventDataPtr event_data,
                        InvokePaymentAppCallback callback) override;
  void InstallAndInvokePaymentApp(
      payments::mojom::PaymentRequestEventDataPtr event_data,
      const std::string& app_name,
      const SkBitmap& app_icon,
      const GURL& sw_js_url,
      const GURL& sw_scope,
      bool sw_use_cache,
      const std::string& method,
      const SupportedDelegations& supported_delegations,
      RegistrationIdCallback registration_id_callback,
      InvokePaymentAppCallback callback) override;
  void UpdatePaymentAppIcon(int64_t registration_id,
                            const std::string& instrument_key,
                            const std::string& name,
                            const std::string& string_encoded_icon,
                            const std::string& method_name,
                            const SupportedDelegations& supported_delegations,
                            UpdatePaymentAppIconCallback callback) override;
  void CanMakePayment(int64_t registration_id,
                      const url::Origin& sw_origin,
                      const std::string& payment_request_id,
                      payments::mojom::CanMakePaymentEventDataPtr event_data,
                      CanMakePaymentCallback callback) override;
  void AbortPayment(int64_t registration_id,
                    const url::Origin& sw_origin,
                    const std::string& payment_request_id,
                    AbortCallback callback) override;
  void SetOpenedWindow(WebContents* payment_handler_web_contents) override;
  void CloseOpenedWindow() override;
  void OnClosingOpenedWindow(
      payments::mojom::PaymentEventResponseType reason) override;

 private:
  explicit PaymentAppProviderImpl(WebContents* payment_request_web_contents);
  friend class WebContentsUserData<PaymentAppProviderImpl>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  scoped_refptr<DevToolsBackgroundServicesContextImpl> GetDevTools(
      const url::Origin& sw_origin);
  void StartServiceWorkerForDispatch(
      int64_t registration_id,
      ServiceWorkerCoreThreadEventDispatcher::ServiceWorkerStartCallback
          callback);
  void OnInstallPaymentApp(
      const url::Origin& sw_origin,
      payments::mojom::PaymentRequestEventDataPtr event_data,
      RegistrationIdCallback registration_id_callback,
      InvokePaymentAppCallback callback,
      int64_t registration_id);

  // Note that constructor of WebContentsObserver is protected.
  class PaymentHandlerWindowObserver : public WebContentsObserver {
   public:
    explicit PaymentHandlerWindowObserver(
        WebContents* payment_handler_web_contents);
    ~PaymentHandlerWindowObserver() override;
  };

  std::unique_ptr<PaymentHandlerWindowObserver> payment_handler_window_;

  // Owns this object.
  WebContents* payment_request_web_contents_;

  // It should be accessed only on the service worker core thread.
  std::unique_ptr<ServiceWorkerCoreThreadEventDispatcher> event_dispatcher_;

  base::WeakPtrFactory<PaymentAppProviderImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_PROVIDER_IMPL_H_
