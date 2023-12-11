// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/respond_with_callback.h"

#include "content/browser/payments/payment_app_provider_impl.h"
#include "content/browser/payments/payment_event_dispatcher.h"

namespace content {

namespace {

using payments::mojom::CanMakePaymentEventResponseType;
using payments::mojom::CanMakePaymentResponsePtr;
using payments::mojom::PaymentEventResponseType;
using payments::mojom::PaymentHandlerResponseCallback;
using payments::mojom::PaymentHandlerResponsePtr;

}  // namespace

mojo::PendingRemote<PaymentHandlerResponseCallback>
RespondWithCallback::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

RespondWithCallback::RespondWithCallback(
    ServiceWorkerMetrics::EventType event_type,
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    base::WeakPtr<PaymentEventDispatcher> event_dispatcher)
    : service_worker_version_(service_worker_version),
      event_dispatcher_(event_dispatcher) {
  request_id_ = service_worker_version->StartRequest(
      event_type, base::BindOnce(&RespondWithCallback::OnServiceWorkerError,
                                 weak_ptr_factory_.GetWeakPtr()));
}

RespondWithCallback::~RespondWithCallback() = default;

void RespondWithCallback::FinishServiceWorkerRequest() {
  service_worker_version_->FinishRequest(request_id_, /*was_handled=*/false);
}

void RespondWithCallback::ClearRespondWithCallbackAndCloseWindow() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!event_dispatcher_)
    return;

  if (base::WeakPtr<PaymentAppProviderImpl> provider =
          event_dispatcher_->payment_app_provider())
    provider->CloseOpenedWindow();

  event_dispatcher_->ResetRespondWithCallback();
}

CanMakePaymentRespondWithCallback::CanMakePaymentRespondWithCallback(
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    base::WeakPtr<PaymentEventDispatcher> event_dispatcher,
    PaymentAppProvider::CanMakePaymentCallback callback)
    : RespondWithCallback(ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT,
                          service_worker_version,
                          event_dispatcher),
      callback_(std::move(callback)) {}

CanMakePaymentRespondWithCallback::~CanMakePaymentRespondWithCallback() =
    default;

void CanMakePaymentRespondWithCallback::OnResponseForCanMakePayment(
    CanMakePaymentResponsePtr response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FinishServiceWorkerRequest();
  std::move(callback_).Run(std::move(response));
  delete this;
}

void CanMakePaymentRespondWithCallback::OnServiceWorkerError(
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(service_worker_status, blink::ServiceWorkerStatusCode::kOk);

  CanMakePaymentEventResponseType response_type =
      CanMakePaymentEventResponseType::BROWSER_ERROR;
  if (service_worker_status ==
      blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected) {
    response_type = CanMakePaymentEventResponseType::REJECT;
  } else if (service_worker_status ==
             blink::ServiceWorkerStatusCode::kErrorTimeout) {
    response_type = CanMakePaymentEventResponseType::TIMEOUT;
  }

  std::move(callback_).Run(
      content::PaymentAppProviderUtil::CreateBlankCanMakePaymentResponse(
          response_type));
  delete this;
}

InvokeRespondWithCallback::InvokeRespondWithCallback(
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    base::WeakPtr<PaymentEventDispatcher> event_dispatcher,
    PaymentAppProvider::InvokePaymentAppCallback callback)
    : RespondWithCallback(ServiceWorkerMetrics::EventType::PAYMENT_REQUEST,
                          service_worker_version,
                          event_dispatcher),
      callback_(std::move(callback)) {}

void InvokeRespondWithCallback::AbortPaymentSinceOpennedWindowClosing(
    PaymentEventResponseType reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  FinishServiceWorkerRequest();
  RespondToPaymentRequestWithErrorAndDeleteSelf(reason);
}

InvokeRespondWithCallback::~InvokeRespondWithCallback() = default;

void InvokeRespondWithCallback::OnResponseForPaymentRequest(
    PaymentHandlerResponsePtr response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FinishServiceWorkerRequest();
  std::move(callback_).Run(std::move(response));
  ClearRespondWithCallbackAndCloseWindow();
}

void InvokeRespondWithCallback::OnServiceWorkerError(
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(service_worker_status, blink::ServiceWorkerStatusCode::kOk);

  PaymentEventResponseType response_type =
      PaymentEventResponseType::PAYMENT_EVENT_BROWSER_ERROR;
  if (service_worker_status ==
      blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected) {
    response_type = PaymentEventResponseType::PAYMENT_EVENT_REJECT;
  } else if (service_worker_status ==
             blink::ServiceWorkerStatusCode::kErrorTimeout) {
    response_type = PaymentEventResponseType::PAYMENT_EVENT_TIMEOUT;
  }

  RespondToPaymentRequestWithErrorAndDeleteSelf(response_type);
}

void InvokeRespondWithCallback::RespondToPaymentRequestWithErrorAndDeleteSelf(
    PaymentEventResponseType response_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback_).Run(
      content::PaymentAppProviderUtil::CreateBlankPaymentHandlerResponse(
          response_type));
  ClearRespondWithCallbackAndCloseWindow();
}

AbortRespondWithCallback::AbortRespondWithCallback(
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    base::WeakPtr<PaymentEventDispatcher> event_dispatcher,
    PaymentAppProvider::AbortCallback callback)
    : RespondWithCallback(ServiceWorkerMetrics::EventType::ABORT_PAYMENT,
                          service_worker_version,
                          event_dispatcher),
      callback_(std::move(callback)) {}

AbortRespondWithCallback::~AbortRespondWithCallback() = default;

// PaymentHandlerResponseCallback implementation.
void AbortRespondWithCallback::OnResponseForAbortPayment(bool payment_aborted) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FinishServiceWorkerRequest();
  std::move(callback_).Run(payment_aborted);

  if (payment_aborted)
    ClearRespondWithCallbackAndCloseWindow();

  delete this;
}

void AbortRespondWithCallback::OnServiceWorkerError(
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(service_worker_status, blink::ServiceWorkerStatusCode::kOk);
  std::move(callback_).Run(/*payment_aborted=*/false);
  // Do not call ClearRespondWithCallbackAndCloseWindow() here, because payment
  // has not been aborted. The service worker either rejected, timed out, or
  // threw a JavaScript exception in the "abortpayment" event, but that does
  // not affect the ongoing "paymentrequest" event.
  delete this;
}

}  // namespace content
