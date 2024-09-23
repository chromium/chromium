// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_RESPOND_WITH_CALLBACK_H_
#define CONTENT_BROWSER_PAYMENTS_RESPOND_WITH_CALLBACK_H_

#include "base/functional/callback_forward.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/payment_app_provider_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"

namespace content {

class PaymentEventDispatcher;
enum class RespondWithCallbackType { kInvoke, kAbort, kCanMakePayment };

// Abstract base class for event callbacks that are invoked when the payment
// handler resolves the promise passed in to TheEvent.respondWith() method.
class RespondWithCallback
    : public payments::mojom::PaymentHandlerResponseCallback {
 public:
  // Disallow copy and assign.
  RespondWithCallback(const RespondWithCallback& other) = delete;
  RespondWithCallback& operator=(const RespondWithCallback& other) = delete;

  mojo::PendingRemote<payments::mojom::PaymentHandlerResponseCallback>
  BindNewPipeAndPassRemote();

 protected:
  RespondWithCallback(
      ServiceWorkerMetrics::EventType event_type,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      base::WeakPtr<PaymentEventDispatcher> event_dispatcher);

  ~RespondWithCallback() override;

  // payments::mojom::PaymentHandlerResponseCallback implementation.
  void OnResponseForCanMakePayment(
      payments::mojom::CanMakePaymentResponsePtr response) override {}

  // payments::mojom::PaymentHandlerResponseCallback implementation.
  void OnResponseForPaymentRequest(
      payments::mojom::PaymentHandlerResponsePtr response) override {}

  // payments::mojom::PaymentHandlerResponseCallback implementation.
  void OnResponseForAbortPayment(bool payment_aborted) override {}

  virtual void OnServiceWorkerError(
      blink::ServiceWorkerStatusCode service_worker_status) = 0;

  void FinishServiceWorkerRequest();
  void ClearRespondWithCallbackAndCloseWindow();

 private:
  int request_id_;
  scoped_refptr<ServiceWorkerVersion> service_worker_version_;
  base::WeakPtr<PaymentEventDispatcher> event_dispatcher_;
  mojo::Receiver<payments::mojom::PaymentHandlerResponseCallback> receiver_{
      this};

  base::WeakPtrFactory<RespondWithCallback> weak_ptr_factory_{this};
};

// Self-deleting callback for "canmakepayment" event. Invoked when the payment
// handler resolves the promise passed into CanMakePaymentEvent.respondWith()
// method.
class CanMakePaymentRespondWithCallback : public RespondWithCallback {
 public:
  CanMakePaymentRespondWithCallback(
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      base::WeakPtr<PaymentEventDispatcher> event_dispatcher,
      PaymentAppProvider::CanMakePaymentCallback callback);
  ~CanMakePaymentRespondWithCallback() override;

  // Disallow copy and assign.
  CanMakePaymentRespondWithCallback(
      const CanMakePaymentRespondWithCallback& other) = delete;
  CanMakePaymentRespondWithCallback& operator=(
      const CanMakePaymentRespondWithCallback& other) = delete;

 private:
  // payments::mojom::PaymentHandlerResponseCallback implementation.
  void OnResponseForCanMakePayment(
      payments::mojom::CanMakePaymentResponsePtr response) override;

  // RespondWithCallback implementation.
  void OnServiceWorkerError(
      blink::ServiceWorkerStatusCode service_worker_status) override;

  PaymentAppProvider::CanMakePaymentCallback callback_;
};

// Self-deleting callback for "paymentrequest" event. Invoked when the payment
// handler resolves the promise passed into PaymentRequestEvent.respondWith()
// method.
class InvokeRespondWithCallback : public RespondWithCallback {
 public:
  InvokeRespondWithCallback(
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      base::WeakPtr<PaymentEventDispatcher> event_dispatcher,
      PaymentAppProvider::InvokePaymentAppCallback callback);
  ~InvokeRespondWithCallback() override;

  // Disallow copy and assign.
  InvokeRespondWithCallback(const InvokeRespondWithCallback& other) = delete;
  InvokeRespondWithCallback& operator=(const InvokeRespondWithCallback& other) =
      delete;

  // Called only for "paymentrequest" event.
  void AbortPaymentSinceOpennedWindowClosing(
      payments::mojom::PaymentEventResponseType reason);

 private:
  // payments::mojom::PaymentHandlerResponseCallback implementation.
  void OnResponseForPaymentRequest(
      payments::mojom::PaymentHandlerResponsePtr response) override;

  // RespondWithCallback implementation.
  void OnServiceWorkerError(
      blink::ServiceWorkerStatusCode service_worker_status) override;

  void RespondToPaymentRequestWithErrorAndDeleteSelf(
      payments::mojom::PaymentEventResponseType response_type);

  PaymentAppProvider::InvokePaymentAppCallback callback_;
};

// Self-deleting callback for "abortpayment" event. Invoked when the payment
// handler resolves the promise passed into AbortPayment.respondWith() method.
class AbortRespondWithCallback : public RespondWithCallback {
 public:
  AbortRespondWithCallback(
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      base::WeakPtr<PaymentEventDispatcher> event_dispatcher,
      PaymentAppProvider::AbortCallback callback);
  ~AbortRespondWithCallback() override;

  // Disallow copy and assign.
  AbortRespondWithCallback(const AbortRespondWithCallback& other) = delete;
  AbortRespondWithCallback& operator=(const AbortRespondWithCallback& other) =
      delete;

 private:
  // payments::mojom::PaymentHandlerResponseCallback implementation.
  void OnResponseForAbortPayment(bool payment_aborted) override;

  // RespondWithCallback implementation.
  void OnServiceWorkerError(
      blink::ServiceWorkerStatusCode service_worker_status) override;

  PaymentAppProvider::AbortCallback callback_;
};

}  // namespace content.

#endif  // CONTENT_BROWSER_PAYMENTS_RESPOND_WITH_CALLBACK_H_
