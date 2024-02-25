// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_event_dispatcher.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/payments/payment_app_provider_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {
namespace {

using payments::mojom::CanMakePaymentEventDataPtr;
using payments::mojom::CanMakePaymentEventResponseType;
using payments::mojom::CanMakePaymentResponsePtr;
using payments::mojom::PaymentEventResponseType;
using payments::mojom::PaymentHandlerResponsePtr;
using payments::mojom::PaymentRequestEventDataPtr;

}  // namespace

namespace {

void DidFindRegistration(
    PaymentEventDispatcher::ServiceWorkerStartCallback callback,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(nullptr, service_worker_status);
    return;
  }

  ServiceWorkerVersion* active_version =
      service_worker_registration->active_version();
  DCHECK(active_version);
  active_version->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::PAYMENT_REQUEST,
      base::BindOnce(std::move(callback),
                     base::WrapRefCounted(active_version)));
}

void OnResponseForPaymentRequest(
    base::WeakPtr<PaymentAppProviderImpl> provider,
    int64_t registration_id,
    const url::Origin& sw_origin,
    const std::string& payment_request_id,
    PaymentAppProvider::InvokePaymentAppCallback callback,
    PaymentHandlerResponsePtr response) {
  DevToolsBackgroundServicesContextImpl* dev_tools =
      provider ? provider->GetDevTools(sw_origin) : nullptr;
  if (dev_tools) {
    std::stringstream response_type;
    response_type << response->response_type;
    dev_tools->LogBackgroundServiceEvent(
        registration_id, blink::StorageKey::CreateFirstParty(sw_origin),
        DevToolsBackgroundService::kPaymentHandler, "Payment response",
        /*instance_id=*/payment_request_id,
        {{"Method Name", response->method_name},
         {"Details", response->stringified_details},
         {"Type", response_type.str()}});
  }

  std::move(callback).Run(std::move(response));
}

void OnResponseForCanMakePayment(
    base::WeakPtr<PaymentAppProviderImpl> provider,
    int64_t registration_id,
    const url::Origin& sw_origin,
    const std::string& payment_request_id,
    PaymentAppProvider::CanMakePaymentCallback callback,
    CanMakePaymentResponsePtr response) {
  DevToolsBackgroundServicesContextImpl* dev_tools =
      provider ? provider->GetDevTools(sw_origin) : nullptr;
  if (dev_tools) {
    std::stringstream response_type;
    response_type << response->response_type;
    std::map<std::string, std::string> data = {
        {"Type", response_type.str()},
        {"Can Make Payment", response->can_make_payment ? "true" : "false"}};
    dev_tools->LogBackgroundServiceEvent(
        registration_id, blink::StorageKey::CreateFirstParty(sw_origin),
        DevToolsBackgroundService::kPaymentHandler, "Can make payment response",
        /*instance_id=*/payment_request_id, data);
  }

  std::move(callback).Run(std::move(response));
}

void OnResponseForAbortPayment(base::WeakPtr<PaymentAppProviderImpl> provider,
                               int64_t registration_id,
                               const url::Origin& sw_origin,
                               const std::string& payment_request_id,
                               PaymentAppProvider::AbortCallback callback,
                               bool payment_aborted) {
  DevToolsBackgroundServicesContextImpl* dev_tools =
      provider ? provider->GetDevTools(sw_origin) : nullptr;
  if (dev_tools) {
    dev_tools->LogBackgroundServiceEvent(
        registration_id, blink::StorageKey::CreateFirstParty(sw_origin),
        DevToolsBackgroundService::kPaymentHandler, "Abort payment response",
        /*instance_id=*/payment_request_id,
        {{"Payment Aborted", payment_aborted ? "true" : "false"}});
  }

  std::move(callback).Run(payment_aborted);
}

}  // namespace

PaymentEventDispatcher::PaymentEventDispatcher() = default;
PaymentEventDispatcher::~PaymentEventDispatcher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void PaymentEventDispatcher::DispatchAbortPaymentEvent(
    PaymentAppProvider::AbortCallback callback,
    scoped_refptr<ServiceWorkerVersion> active_version,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  DCHECK(active_version);

  int event_finish_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT, base::DoNothing());

  // This object self-deletes after either success or error callback is
  // invoked.
  RespondWithCallback* respond_with_callback = new AbortRespondWithCallback(
      active_version, weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  active_version->endpoint()->DispatchAbortPaymentEvent(
      respond_with_callback->BindNewPipeAndPassRemote(),
      active_version->CreateSimpleEventCallback(event_finish_id));
}

void PaymentEventDispatcher::AbortPayment(
    int64_t registration_id,
    const url::Origin& sw_origin,
    const std::string& payment_request_id,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    PaymentAppProvider::AbortCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  service_worker_context->FindReadyRegistrationForIdOnly(
      registration_id,
      base::BindOnce(
          &DidFindRegistration,
          base::BindOnce(
              &PaymentEventDispatcher::DispatchAbortPaymentEvent,
              weak_ptr_factory_.GetWeakPtr(),
              base::BindOnce(&OnResponseForAbortPayment, payment_app_provider(),
                             registration_id, sw_origin, payment_request_id,
                             std::move(callback)))));
}

void PaymentEventDispatcher::DispatchCanMakePaymentEvent(
    CanMakePaymentEventDataPtr event_data,
    PaymentAppProvider::CanMakePaymentCallback callback,
    scoped_refptr<ServiceWorkerVersion> active_version,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            PaymentAppProviderUtil::CreateBlankCanMakePaymentResponse(
                CanMakePaymentEventResponseType::BROWSER_ERROR)));
    return;
  }

  DCHECK(active_version);

  int event_finish_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT, base::DoNothing());

  // This object self-deletes after either success or error callback is
  // invoked.
  RespondWithCallback* respond_with_callback =
      new CanMakePaymentRespondWithCallback(
          active_version, weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  active_version->endpoint()->DispatchCanMakePaymentEvent(
      std::move(event_data), respond_with_callback->BindNewPipeAndPassRemote(),
      active_version->CreateSimpleEventCallback(event_finish_id));
}

void PaymentEventDispatcher::CanMakePayment(
    int64_t registration_id,
    const url::Origin& sw_origin,
    const std::string& payment_request_id,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    CanMakePaymentEventDataPtr event_data,
    PaymentAppProvider::CanMakePaymentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  service_worker_context->FindReadyRegistrationForIdOnly(
      registration_id,
      base::BindOnce(
          &DidFindRegistration,
          base::BindOnce(
              &PaymentEventDispatcher::DispatchCanMakePaymentEvent,
              weak_ptr_factory_.GetWeakPtr(), std::move(event_data),
              base::BindOnce(&OnResponseForCanMakePayment,
                             payment_app_provider(), registration_id, sw_origin,
                             payment_request_id, std::move(callback)))));
}

void PaymentEventDispatcher::DispatchPaymentRequestEvent(
    PaymentRequestEventDataPtr event_data,
    PaymentAppProvider::InvokePaymentAppCallback callback,
    scoped_refptr<ServiceWorkerVersion> active_version,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            content::PaymentAppProviderUtil::CreateBlankPaymentHandlerResponse(
                PaymentEventResponseType::PAYMENT_EVENT_BROWSER_ERROR)));
    return;
  }

  DCHECK(active_version);

  int event_finish_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::PAYMENT_REQUEST, base::DoNothing());

  invoke_respond_with_callback_ = std::make_unique<InvokeRespondWithCallback>(
      active_version, weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  active_version->endpoint()->DispatchPaymentRequestEvent(
      std::move(event_data),
      invoke_respond_with_callback_->BindNewPipeAndPassRemote(),
      active_version->CreateSimpleEventCallback(event_finish_id));
}

void PaymentEventDispatcher::InvokePayment(
    int64_t registration_id,
    const url::Origin& sw_origin,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    PaymentRequestEventDataPtr event_data,
    PaymentAppProvider::InvokePaymentAppCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  service_worker_context->FindReadyRegistrationForIdOnly(
      registration_id,
      base::BindOnce(
          &DidFindRegistration,
          base::BindOnce(
              &PaymentEventDispatcher::DispatchPaymentRequestEvent,
              weak_ptr_factory_.GetWeakPtr(), std::move(event_data),
              base::BindOnce(&OnResponseForPaymentRequest,
                             payment_app_provider(), registration_id, sw_origin,
                             event_data->payment_request_id,
                             std::move(callback)))));
}

void PaymentEventDispatcher::FindRegistration(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    int64_t registration_id,
    PaymentEventDispatcher::ServiceWorkerStartCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  service_worker_context->FindReadyRegistrationForIdOnly(
      registration_id,
      base::BindOnce(&DidFindRegistration, std::move(callback)));
}

void PaymentEventDispatcher::OnClosingOpenedWindow(
    PaymentEventResponseType reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  InvokeRespondWithCallback* callback = invoke_respond_with_callback_.get();

  if (callback)
    callback->AbortPaymentSinceOpennedWindowClosing(reason);
}

void PaymentEventDispatcher::ResetRespondWithCallback() {
  invoke_respond_with_callback_.reset();
}

base::WeakPtr<PaymentEventDispatcher> PaymentEventDispatcher::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace content.
