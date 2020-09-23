// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/service_worker_core_thread_event_dispatcher.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/payments_validators.h"
#include "content/browser/payments/payment_app_provider_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"

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

void DidFindRegistrationOnCoreThread(
    ServiceWorkerCoreThreadEventDispatcher::ServiceWorkerStartCallback callback,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

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

void OnResponseForPaymentRequestOnUiThread(
    scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools,
    int64_t registration_id,
    const url::Origin& sw_origin,
    const std::string& payment_request_id,
    PaymentAppProvider::InvokePaymentAppCallback callback,
    PaymentHandlerResponsePtr response) {
  if (dev_tools) {
    std::stringstream response_type;
    response_type << response->response_type;
    dev_tools->LogBackgroundServiceEvent(
        registration_id, sw_origin, DevToolsBackgroundService::kPaymentHandler,
        "Payment response",
        /*instance_id=*/payment_request_id,
        {{"Method Name", response->method_name},
         {"Details", response->stringified_details},
         {"Type", response_type.str()}});
  }

  std::move(callback).Run(std::move(response));
}

void OnResponseForCanMakePaymentOnUiThread(
    scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools,
    int64_t registration_id,
    const url::Origin& sw_origin,
    const std::string& payment_request_id,
    PaymentAppProvider::CanMakePaymentCallback callback,
    CanMakePaymentResponsePtr response) {
  std::string error_message;
  if (response->account_balance && !response->account_balance->empty() &&
      !payments::PaymentsValidators::IsValidAmountFormat(
          *response->account_balance, &error_message)) {
    mojo::ReportBadMessage(
        payments::errors::kCanMakePaymentEventInvalidAccountBalanceValue);
    response =
        content::PaymentAppProviderUtil::CreateBlankCanMakePaymentResponse(
            CanMakePaymentEventResponseType::INVALID_ACCOUNT_BALANCE_VALUE);
  }

  if (dev_tools) {
    std::stringstream response_type;
    response_type << response->response_type;
    std::map<std::string, std::string> data = {
        {"Type", response_type.str()},
        {"Can Make Payment", response->can_make_payment ? "true" : "false"}};
    if (base::FeatureList::IsEnabled(features::kWebPaymentsMinimalUI)) {
      data["Ready for Minimal UI"] =
          response->ready_for_minimal_ui ? "true" : "false";
      data["Account Balance"] =
          response->account_balance ? *response->account_balance : "";
    }
    dev_tools->LogBackgroundServiceEvent(
        registration_id, sw_origin, DevToolsBackgroundService::kPaymentHandler,
        "Can make payment response",
        /*instance_id=*/payment_request_id, data);
  }

  std::move(callback).Run(std::move(response));
}

void OnResponseForAbortPaymentOnUiThread(
    scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools,
    int64_t registration_id,
    const url::Origin& sw_origin,
    const std::string& payment_request_id,
    PaymentAppProvider::AbortCallback callback,
    bool payment_aborted) {
  if (dev_tools) {
    dev_tools->LogBackgroundServiceEvent(
        registration_id, sw_origin, DevToolsBackgroundService::kPaymentHandler,
        "Abort payment response",
        /*instance_id=*/payment_request_id,
        {{"Payment Aborted", payment_aborted ? "true" : "false"}});
  }

  std::move(callback).Run(payment_aborted);
}

}  // namespace

ServiceWorkerCoreThreadEventDispatcher::ServiceWorkerCoreThreadEventDispatcher(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

ServiceWorkerCoreThreadEventDispatcher::
    ~ServiceWorkerCoreThreadEventDispatcher() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
}

void ServiceWorkerCoreThreadEventDispatcher::DispatchAbortPaymentEvent(
    PaymentAppProvider::AbortCallback callback,
    scoped_refptr<ServiceWorkerVersion> active_version,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (!web_contents())
    return;

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
      web_contents(), active_version, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback));

  active_version->endpoint()->DispatchAbortPaymentEvent(
      respond_with_callback->BindNewPipeAndPassRemote(),
      active_version->CreateSimpleEventCallback(event_finish_id));
}

void ServiceWorkerCoreThreadEventDispatcher::AbortPaymentOnCoreThread(
    int64_t registration_id,
    const url::Origin& sw_origin,
    const std::string& payment_request_id,
    scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    PaymentAppProvider::AbortCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  service_worker_context->FindReadyRegistrationForIdOnly(
      registration_id,
      base::BindOnce(
          &DidFindRegistrationOnCoreThread,
          base::BindOnce(
              &ServiceWorkerCoreThreadEventDispatcher::
                  DispatchAbortPaymentEvent,
              weak_ptr_factory_.GetWeakPtr(),
              base::BindOnce(&OnResponseForAbortPaymentOnUiThread, dev_tools,
                             registration_id, sw_origin, payment_request_id,
                             std::move(callback)))));
}

void ServiceWorkerCoreThreadEventDispatcher::DispatchCanMakePaymentEvent(
    CanMakePaymentEventDataPtr event_data,
    PaymentAppProvider::CanMakePaymentCallback callback,
    scoped_refptr<ServiceWorkerVersion> active_version,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (!web_contents())
    return;

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
      new CanMakePaymentRespondWithCallback(web_contents(), active_version,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            std::move(callback));

  active_version->endpoint()->DispatchCanMakePaymentEvent(
      std::move(event_data), respond_with_callback->BindNewPipeAndPassRemote(),
      active_version->CreateSimpleEventCallback(event_finish_id));
}

void ServiceWorkerCoreThreadEventDispatcher::CanMakePaymentOnCoreThread(
    int64_t registration_id,
    const url::Origin& sw_origin,
    const std::string& payment_request_id,
    scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    CanMakePaymentEventDataPtr event_data,
    PaymentAppProvider::CanMakePaymentCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  service_worker_context->FindReadyRegistrationForIdOnly(
      registration_id,
      base::BindOnce(
          &DidFindRegistrationOnCoreThread,
          base::BindOnce(
              &ServiceWorkerCoreThreadEventDispatcher::
                  DispatchCanMakePaymentEvent,
              weak_ptr_factory_.GetWeakPtr(), std::move(event_data),
              base::BindOnce(&OnResponseForCanMakePaymentOnUiThread, dev_tools,
                             registration_id, sw_origin, payment_request_id,
                             std::move(callback)))));
}

void ServiceWorkerCoreThreadEventDispatcher::DispatchPaymentRequestEvent(
    PaymentRequestEventDataPtr event_data,
    PaymentAppProvider::InvokePaymentAppCallback callback,
    scoped_refptr<ServiceWorkerVersion> active_version,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (!web_contents())
    return;

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
      web_contents(), active_version, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback));

  active_version->endpoint()->DispatchPaymentRequestEvent(
      std::move(event_data),
      invoke_respond_with_callback_->BindNewPipeAndPassRemote(),
      active_version->CreateSimpleEventCallback(event_finish_id));
}

void ServiceWorkerCoreThreadEventDispatcher::InvokePaymentOnCoreThread(
    int64_t registration_id,
    const url::Origin& sw_origin,
    scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    PaymentRequestEventDataPtr event_data,
    PaymentAppProvider::InvokePaymentAppCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  service_worker_context->FindReadyRegistrationForIdOnly(
      registration_id,
      base::BindOnce(
          &DidFindRegistrationOnCoreThread,
          base::BindOnce(&ServiceWorkerCoreThreadEventDispatcher::
                             DispatchPaymentRequestEvent,
                         weak_ptr_factory_.GetWeakPtr(), std::move(event_data),
                         base::BindOnce(&OnResponseForPaymentRequestOnUiThread,
                                        dev_tools, registration_id, sw_origin,
                                        event_data->payment_request_id,
                                        std::move(callback)))));
}

void ServiceWorkerCoreThreadEventDispatcher::FindRegistrationOnCoreThread(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    int64_t registration_id,
    ServiceWorkerCoreThreadEventDispatcher::ServiceWorkerStartCallback
        callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  service_worker_context->FindReadyRegistrationForIdOnly(
      registration_id,
      base::BindOnce(&DidFindRegistrationOnCoreThread, std::move(callback)));
}

void ServiceWorkerCoreThreadEventDispatcher::OnClosingOpenedWindowOnCoreThread(
    PaymentEventResponseType reason) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  InvokeRespondWithCallback* callback = invoke_respond_with_callback_.get();

  if (callback)
    callback->AbortPaymentSinceOpennedWindowClosing(reason);
}

void ServiceWorkerCoreThreadEventDispatcher::ResetRespondWithCallback() {
  RunOrPostTaskOnThread(FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
                        base::BindOnce(&ServiceWorkerCoreThreadEventDispatcher::
                                           ResetRespondWithCallbackCoreThread,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void ServiceWorkerCoreThreadEventDispatcher::
    ResetRespondWithCallbackCoreThread() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  invoke_respond_with_callback_.reset();
}

base::WeakPtr<ServiceWorkerCoreThreadEventDispatcher>
ServiceWorkerCoreThreadEventDispatcher::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace content.
