// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_EVENT_DISPATCHER_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_EVENT_DISPATCHER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/payments/respond_with_callback.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace content {

class PaymentAppProviderImpl;

// Lives on the UI thread.
class PaymentEventDispatcher {
 public:
  PaymentEventDispatcher();
  ~PaymentEventDispatcher();

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

  void AbortPayment(
      int64_t registration_id,
      const url::Origin& sw_origin,
      const std::string& payment_request_id,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      PaymentAppProvider::AbortCallback callback);

  void CanMakePayment(
      int64_t registration_id,
      const url::Origin& sw_origin,
      const std::string& payment_request_id,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      payments::mojom::CanMakePaymentEventDataPtr event_data,
      PaymentAppProvider::CanMakePaymentCallback callback);

  void InvokePayment(
      int64_t registration_id,
      const url::Origin& sw_origin,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      payments::mojom::PaymentRequestEventDataPtr event_data,
      PaymentAppProvider::InvokePaymentAppCallback callback);

  void FindRegistration(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      int64_t registration_id,
      PaymentEventDispatcher::ServiceWorkerStartCallback callback);

  void OnClosingOpenedWindow(payments::mojom::PaymentEventResponseType reason);

  void ResetRespondWithCallback();

  base::WeakPtr<PaymentEventDispatcher> GetWeakPtr();

 private:
  void DispatchAbortPaymentEvent(
      PaymentAppProvider::AbortCallback callback,
      scoped_refptr<ServiceWorkerVersion> active_version,
      blink::ServiceWorkerStatusCode service_worker_status);

  void DispatchCanMakePaymentEvent(
      payments::mojom::CanMakePaymentEventDataPtr event_data,
      PaymentAppProvider::CanMakePaymentCallback callback,
      scoped_refptr<ServiceWorkerVersion> active_version,
      blink::ServiceWorkerStatusCode service_worker_status);

  void DispatchPaymentRequestEvent(
      payments::mojom::PaymentRequestEventDataPtr event_data,
      PaymentAppProvider::InvokePaymentAppCallback callback,
      scoped_refptr<ServiceWorkerVersion> active_version,
      blink::ServiceWorkerStatusCode service_worker_status);

  std::unique_ptr<InvokeRespondWithCallback> invoke_respond_with_callback_;

  base::WeakPtr<PaymentAppProviderImpl> payment_app_provider_;

  base::WeakPtrFactory<PaymentEventDispatcher> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_EVENT_DISPATCHER_H_
