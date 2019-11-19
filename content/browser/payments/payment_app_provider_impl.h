// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_PROVIDER_IMPL_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "content/common/content_export.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class CONTENT_EXPORT PaymentAppProviderImpl : public PaymentAppProvider {
 public:
  static PaymentAppProviderImpl* GetInstance();

  // PaymentAppProvider implementation:
  // Should be accessed only on the UI thread.
  void GetAllPaymentApps(BrowserContext* browser_context,
                         GetAllPaymentAppsCallback callback) override;
  void InvokePaymentApp(BrowserContext* browser_context,
                        int64_t registration_id,
                        const url::Origin& sw_origin,
                        payments::mojom::PaymentRequestEventDataPtr event_data,
                        InvokePaymentAppCallback callback) override;
  void InstallAndInvokePaymentApp(
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
      InvokePaymentAppCallback callback) override;
  void CanMakePayment(BrowserContext* browser_context,
                      int64_t registration_id,
                      const url::Origin& sw_origin,
                      const std::string& payment_request_id,
                      payments::mojom::CanMakePaymentEventDataPtr event_data,
                      PaymentEventResultCallback callback) override;
  void AbortPayment(BrowserContext* browser_context,
                    int64_t registration_id,
                    const url::Origin& sw_origin,
                    const std::string& payment_request_id,
                    PaymentEventResultCallback callback) override;
  void SetOpenedWindow(WebContents* web_contents) override;
  void CloseOpenedWindow(BrowserContext* browser_context) override;
  void OnClosingOpenedWindow(
      BrowserContext* browser_context,
      payments::mojom::PaymentEventResponseType reason) override;
  bool IsValidInstallablePaymentApp(const GURL& manifest_url,
                                    const GURL& sw_js_url,
                                    const GURL& sw_scope,
                                    std::string* error_message) override;

 private:
  PaymentAppProviderImpl();
  ~PaymentAppProviderImpl() override;

  friend struct base::DefaultSingletonTraits<PaymentAppProviderImpl>;

  // Note that constructor of WebContentsObserver is protected.
  class PaymentHandlerWindowObserver : public WebContentsObserver {
   public:
    explicit PaymentHandlerWindowObserver(WebContents* web_contents);
    ~PaymentHandlerWindowObserver() override;
  };

  // Map to maintain at most one opened window per browser context.
  std::map<BrowserContext*, std::unique_ptr<PaymentHandlerWindowObserver>>
      payment_handler_windows_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppProviderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_PROVIDER_IMPL_H_
