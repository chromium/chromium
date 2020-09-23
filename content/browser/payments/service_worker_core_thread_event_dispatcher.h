// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_SERVICE_WORKER_CORE_THREAD_EVENT_DISPATCHER_H_
#define CONTENT_BROWSER_PAYMENTS_SERVICE_WORKER_CORE_THREAD_EVENT_DISPATCHER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/payments/respond_with_callback.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace content {

class WebContents;
class PaymentAppProviderImpl;

// All of the methods and the destructor should be running on the
// service worker core thread.
class ServiceWorkerCoreThreadEventDispatcher : public WebContentsObserver {
 public:
  explicit ServiceWorkerCoreThreadEventDispatcher(WebContents* web_contents);

  ~ServiceWorkerCoreThreadEventDispatcher() override;

  using ServiceWorkerStartCallback =
      base::OnceCallback<void(scoped_refptr<ServiceWorkerVersion>,
                              blink::ServiceWorkerStatusCode)>;

  void set_payment_app_provider(
      base::WeakPtr<PaymentAppProviderImpl> payment_app_provider) {
    payment_app_provider_ = payment_app_provider;
  }

  base::WeakPtr<PaymentAppProviderImpl> payment_app_provider() const {
    return payment_app_provider_;
  }

  void AbortPaymentOnCoreThread(
      int64_t registration_id,
      const url::Origin& sw_origin,
      const std::string& payment_request_id,
      scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      PaymentAppProvider::AbortCallback callback);

  void CanMakePaymentOnCoreThread(
      int64_t registration_id,
      const url::Origin& sw_origin,
      const std::string& payment_request_id,
      scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      payments::mojom::CanMakePaymentEventDataPtr event_data,
      PaymentAppProvider::CanMakePaymentCallback callback);

  void InvokePaymentOnCoreThread(
      int64_t registration_id,
      const url::Origin& sw_origin,
      scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      payments::mojom::PaymentRequestEventDataPtr event_data,
      PaymentAppProvider::InvokePaymentAppCallback callback);

  void FindRegistrationOnCoreThread(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      int64_t registration_id,
      ServiceWorkerCoreThreadEventDispatcher::ServiceWorkerStartCallback
          callback);

  void OnClosingOpenedWindowOnCoreThread(
      payments::mojom::PaymentEventResponseType reason);

  void ResetRespondWithCallback();

  base::WeakPtr<ServiceWorkerCoreThreadEventDispatcher> GetWeakPtr();

 private:
  // AbortCallback require to be run on the UI thread.
  void DispatchAbortPaymentEvent(
      PaymentAppProvider::AbortCallback callback,
      scoped_refptr<ServiceWorkerVersion> active_version,
      blink::ServiceWorkerStatusCode service_worker_status);

  // CanMakePaymentCallback require to be run on the UI thread.
  void DispatchCanMakePaymentEvent(
      payments::mojom::CanMakePaymentEventDataPtr event_data,
      PaymentAppProvider::CanMakePaymentCallback callback,
      scoped_refptr<ServiceWorkerVersion> active_version,
      blink::ServiceWorkerStatusCode service_worker_status);

  // InvokePaymentAppCallback require to be run on the UI thread.
  void DispatchPaymentRequestEvent(
      payments::mojom::PaymentRequestEventDataPtr event_data,
      PaymentAppProvider::InvokePaymentAppCallback callback,
      scoped_refptr<ServiceWorkerVersion> active_version,
      blink::ServiceWorkerStatusCode service_worker_status);

  void ResetRespondWithCallbackCoreThread();

  std::unique_ptr<InvokeRespondWithCallback> invoke_respond_with_callback_;

  // payment_app_provider_ require to be run on the UI thread.
  base::WeakPtr<PaymentAppProviderImpl> payment_app_provider_;

  base::WeakPtrFactory<ServiceWorkerCoreThreadEventDispatcher>
      weak_ptr_factory_{this};
};

}  // namespace content.

#endif  // CONTENT_BROWSER_PAYMENTS_SERVICE_WORKER_CORE_THREAD_EVENT_DISPATCHER_H_
