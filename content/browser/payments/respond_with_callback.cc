// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/respond_with_callback.h"

#include "content/browser/payments/payment_app_provider_impl.h"
#include "content/browser/payments/service_worker_core_thread_event_dispatcher.h"

namespace content {

namespace {

using payments::mojom::CanMakePaymentEventResponseType;
using payments::mojom::CanMakePaymentResponsePtr;
using payments::mojom::PaymentEventResponseType;
using payments::mojom::PaymentHandlerResponseCallback;
using payments::mojom::PaymentHandlerResponsePtr;

void CloseOpenedWindowUiThread(
    base::WeakPtr<PaymentAppProviderImpl> payment_app_provider) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (payment_app_provider)
    payment_app_provider->CloseOpenedWindow();
}

}  // namespace

mojo::PendingRemote<PaymentHandlerResponseCallback>
RespondWithCallback::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

RespondWithCallback::RespondWithCallback(
    WebContents* web_contents,
    ServiceWorkerMetrics::EventType event_type,
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    base::WeakPtr<ServiceWorkerCoreThreadEventDispatcher> event_dispatcher)
    : WebContentsObserver(web_contents),
      service_worker_version_(service_worker_version),
      event_dispatcher_(event_dispatcher) {
  request_id_ = service_worker_version->StartRequest(
      event_type, base::BindOnce(&RespondWithCallback::OnServiceWorkerError,
                                 weak_ptr_factory_.GetWeakPtr()));
}

RespondWithCallback::~RespondWithCallback() = default;

void RespondWithCallback::FinishServiceWorkerRequest() {
  service_worker_version_->FinishRequest(request_id_, /*was_handled=*/false);
}

void RespondWithCallback::MaybeRecordTimeoutMetric(
    blink::ServiceWorkerStatusCode status) {
  if (status == blink::ServiceWorkerStatusCode::kErrorTimeout) {
    UMA_HISTOGRAM_BOOLEAN("PaymentRequest.ServiceWorkerStatusCodeTimeout",
                          true);
  }
}

void RespondWithCallback::ClearRespondWithCallbackAndCloseWindow() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (!web_contents())
    return;

  if (!event_dispatcher_)
    return;

  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&CloseOpenedWindowUiThread,
                     event_dispatcher_->payment_app_provider()));

  event_dispatcher_->ResetRespondWithCallback();
}

CanMakePaymentRespondWithCallback::CanMakePaymentRespondWithCallback(
    WebContents* web_contents,
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    base::WeakPtr<ServiceWorkerCoreThreadEventDispatcher> event_dispatcher,
    PaymentAppProvider::CanMakePaymentCallback callback)
    : RespondWithCallback(web_contents,
                          ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT,
                          service_worker_version,
                          event_dispatcher),
      callback_(std::move(callback)) {}

CanMakePaymentRespondWithCallback::~CanMakePaymentRespondWithCallback() =
    default;

void CanMakePaymentRespondWithCallback::OnResponseForCanMakePayment(
    CanMakePaymentResponsePtr response) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  FinishServiceWorkerRequest();
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(std::move(callback_), std::move(response)));
  delete this;
}

void CanMakePaymentRespondWithCallback::OnServiceWorkerError(
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK_NE(service_worker_status, blink::ServiceWorkerStatusCode::kOk);
  MaybeRecordTimeoutMetric(service_worker_status);

  CanMakePaymentEventResponseType response_type =
      CanMakePaymentEventResponseType::BROWSER_ERROR;
  if (service_worker_status ==
      blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected) {
    response_type = CanMakePaymentEventResponseType::REJECT;
  } else if (service_worker_status ==
             blink::ServiceWorkerStatusCode::kErrorTimeout) {
    response_type = CanMakePaymentEventResponseType::TIMEOUT;
  }

  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(
          std::move(callback_),
          content::PaymentAppProviderUtil::CreateBlankCanMakePaymentResponse(
              response_type)));
  delete this;
}

InvokeRespondWithCallback::InvokeRespondWithCallback(
    WebContents* web_contents,
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    base::WeakPtr<ServiceWorkerCoreThreadEventDispatcher> event_dispatcher,
    PaymentAppProvider::InvokePaymentAppCallback callback)
    : RespondWithCallback(web_contents,
                          ServiceWorkerMetrics::EventType::PAYMENT_REQUEST,
                          service_worker_version,
                          event_dispatcher),
      callback_(std::move(callback)) {}

void InvokeRespondWithCallback::AbortPaymentSinceOpennedWindowClosing(
    PaymentEventResponseType reason) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  FinishServiceWorkerRequest();
  RespondToPaymentRequestWithErrorAndDeleteSelf(reason);
}

InvokeRespondWithCallback::~InvokeRespondWithCallback() = default;

void InvokeRespondWithCallback::OnResponseForPaymentRequest(
    PaymentHandlerResponsePtr response) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  FinishServiceWorkerRequest();
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(std::move(callback_), std::move(response)));
  ClearRespondWithCallbackAndCloseWindow();
}

void InvokeRespondWithCallback::OnServiceWorkerError(
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK_NE(service_worker_status, blink::ServiceWorkerStatusCode::kOk);
  MaybeRecordTimeoutMetric(service_worker_status);

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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(
          std::move(callback_),
          content::PaymentAppProviderUtil::CreateBlankPaymentHandlerResponse(
              response_type)));
  ClearRespondWithCallbackAndCloseWindow();
}

AbortRespondWithCallback::AbortRespondWithCallback(
    WebContents* web_contents,
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    base::WeakPtr<ServiceWorkerCoreThreadEventDispatcher> event_dispatcher,
    PaymentAppProvider::AbortCallback callback)
    : RespondWithCallback(web_contents,
                          ServiceWorkerMetrics::EventType::ABORT_PAYMENT,
                          service_worker_version,
                          event_dispatcher),
      callback_(std::move(callback)) {}

AbortRespondWithCallback::~AbortRespondWithCallback() = default;

// PaymentHandlerResponseCallback implementation.
void AbortRespondWithCallback::OnResponseForAbortPayment(bool payment_aborted) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  FinishServiceWorkerRequest();
  RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI,
                        base::BindOnce(std::move(callback_), payment_aborted));

  if (payment_aborted)
    ClearRespondWithCallbackAndCloseWindow();

  delete this;
}

void AbortRespondWithCallback::OnServiceWorkerError(
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK_NE(service_worker_status, blink::ServiceWorkerStatusCode::kOk);
  MaybeRecordTimeoutMetric(service_worker_status);
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(std::move(callback_), /*payment_aborted=*/false));
  // Do not call ClearRespondWithCallbackAndCloseWindow() here, because payment
  // has not been aborted. The service worker either rejected, timed out, or
  // threw a JavaScript exception in the "abortpayment" event, but that does
  // not affect the ongoing "paymentrequest" event.
  delete this;
}

}  // namespace content.
