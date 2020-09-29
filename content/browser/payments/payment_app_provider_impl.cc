// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_provider_impl.h"

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/token.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/payments/payment_app_installer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_background_services_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/mojom/base/time.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container_type.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace content {
namespace {

using payments::mojom::CanMakePaymentEventDataPtr;
using payments::mojom::PaymentDetailsModifierPtr;
using payments::mojom::PaymentEventResponseType;
using payments::mojom::PaymentHandlerStatus;
using payments::mojom::PaymentMethodDataPtr;
using payments::mojom::PaymentRequestEventDataPtr;

void DidUpdatePaymentAppIconOnCoreThread(
    PaymentAppProvider::UpdatePaymentAppIconCallback callback,
    PaymentHandlerStatus status) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status));
}

void UpdatePaymentAppIconOnCoreThread(
    scoped_refptr<PaymentAppContextImpl> payment_app_context,
    int64_t registration_id,
    const std::string& instrument_key,
    const std::string& name,
    const std::string& string_encoded_icon,
    const std::string& method_name,
    const SupportedDelegations& supported_delegations,
    PaymentAppProvider::UpdatePaymentAppIconCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  payment_app_context->payment_app_database()
      ->SetPaymentAppInfoForRegisteredServiceWorker(
          registration_id, instrument_key, name, string_encoded_icon,
          method_name, supported_delegations,
          base::BindOnce(&DidUpdatePaymentAppIconOnCoreThread,
                         std::move(callback)));
}

void AddMethodDataToMap(const std::vector<PaymentMethodDataPtr>& method_data,
                        std::map<std::string, std::string>* out) {
  for (size_t i = 0; i < method_data.size(); ++i) {
    std::string counter =
        method_data.size() == 1 ? "" : " #" + base::NumberToString(i);
    out->emplace("Method Name" + counter, method_data[i]->supported_method);
    out->emplace("Method Data" + counter, method_data[i]->stringified_data);
  }
}

void AddModifiersToMap(const std::vector<PaymentDetailsModifierPtr>& modifiers,
                       std::map<std::string, std::string>* out) {
  for (size_t i = 0; i < modifiers.size(); ++i) {
    std::string prefix =
        "Modifier" +
        (modifiers.size() == 1 ? "" : " #" + base::NumberToString(i));
    out->emplace(prefix + " Method Name",
                 modifiers[i]->method_data->supported_method);
    out->emplace(prefix + " Method Data",
                 modifiers[i]->method_data->stringified_data);
    if (!modifiers[i]->total)
      continue;
    out->emplace(prefix + " Total Currency",
                 modifiers[i]->total->amount->currency);
    out->emplace(prefix + " Total Value", modifiers[i]->total->amount->value);
  }
}

}  // namespace

// static
PaymentAppProvider* PaymentAppProvider::GetOrCreateForWebContents(
    WebContents* payment_request_web_contents) {
  return PaymentAppProviderImpl::GetOrCreateForWebContents(
      payment_request_web_contents);
}

// static
PaymentAppProviderImpl* PaymentAppProviderImpl::GetOrCreateForWebContents(
    WebContents* payment_request_web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* data =
      PaymentAppProviderImpl::FromWebContents(payment_request_web_contents);
  if (!data)
    PaymentAppProviderImpl::CreateForWebContents(payment_request_web_contents);

  return PaymentAppProviderImpl::FromWebContents(payment_request_web_contents);
}

void PaymentAppProviderImpl::InvokePaymentApp(
    int64_t registration_id,
    const url::Origin& sw_origin,
    PaymentRequestEventDataPtr event_data,
    InvokePaymentAppCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(payment_request_web_contents_);

  scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools =
      GetDevTools(sw_origin);
  if (dev_tools) {
    std::map<std::string, std::string> data = {
        {"Merchant Top Origin", event_data->top_origin.spec()},
        {"Merchant Payment Request Origin",
         event_data->payment_request_origin.spec()},
        {"Total Currency", event_data->total->currency},
        {"Total Value", event_data->total->value},
        {"Instrument Key", event_data->instrument_key},
    };
    AddMethodDataToMap(event_data->method_data, &data);
    AddModifiersToMap(event_data->modifiers, &data);
    dev_tools->LogBackgroundServiceEvent(
        registration_id, sw_origin, DevToolsBackgroundService::kPaymentHandler,
        "Payment request",
        /*instance_id=*/event_data->payment_request_id, data);
  }

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(
          payment_request_web_contents_->GetBrowserContext()));
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      partition->GetServiceWorkerContext();

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          &ServiceWorkerCoreThreadEventDispatcher::InvokePaymentOnCoreThread,
          event_dispatcher_->GetWeakPtr(), registration_id, sw_origin,
          std::move(dev_tools), std::move(service_worker_context),
          std::move(event_data), std::move(callback)));
}

void PaymentAppProviderImpl::InstallAndInvokePaymentApp(
    PaymentRequestEventDataPtr event_data,
    const std::string& app_name,
    const SkBitmap& app_icon,
    const GURL& sw_js_url,
    const GURL& sw_scope,
    bool sw_use_cache,
    const std::string& method,
    const SupportedDelegations& supported_delegations,
    RegistrationIdCallback registration_id_callback,
    InvokePaymentAppCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(payment_request_web_contents_);

  if (!sw_js_url.is_valid() || !sw_scope.is_valid() || method.empty()) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            PaymentAppProviderUtil::CreateBlankPaymentHandlerResponse(
                PaymentEventResponseType::PAYMENT_EVENT_BROWSER_ERROR)));
    return;
  }

  std::string string_encoded_icon;
  if (!app_icon.empty()) {
    gfx::Image decoded_image = gfx::Image::CreateFrom1xBitmap(app_icon);
    scoped_refptr<base::RefCountedMemory> raw_data =
        decoded_image.As1xPNGBytes();
    base::Base64Encode(
        base::StringPiece(raw_data->front_as<char>(), raw_data->size()),
        &string_encoded_icon);
  }

  PaymentAppInstaller::Install(
      payment_request_web_contents_, app_name, string_encoded_icon, sw_js_url,
      sw_scope, sw_use_cache, method, supported_delegations,
      base::BindOnce(&PaymentAppProviderImpl::OnInstallPaymentApp,
                     weak_ptr_factory_.GetWeakPtr(),
                     url::Origin::Create(sw_scope), std::move(event_data),
                     std::move(registration_id_callback), std::move(callback)));
}

void PaymentAppProviderImpl::UpdatePaymentAppIcon(
    int64_t registration_id,
    const std::string& instrument_key,
    const std::string& name,
    const std::string& string_encoded_icon,
    const std::string& method_name,
    const SupportedDelegations& supported_delegations,
    PaymentAppProvider::UpdatePaymentAppIconCallback callback) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(
          payment_request_web_contents_->GetBrowserContext()));
  scoped_refptr<PaymentAppContextImpl> payment_app_context =
      partition->GetPaymentAppContext();

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&UpdatePaymentAppIconOnCoreThread, payment_app_context,
                     registration_id, instrument_key, name, string_encoded_icon,
                     method_name, supported_delegations, std::move(callback)));
}

void PaymentAppProviderImpl::CanMakePayment(
    int64_t registration_id,
    const url::Origin& sw_origin,
    const std::string& payment_request_id,
    CanMakePaymentEventDataPtr event_data,
    CanMakePaymentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(payment_request_web_contents_);

  scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools =
      GetDevTools(sw_origin);
  if (dev_tools) {
    std::map<std::string, std::string> data = {
        {"Merchant Top Origin", event_data->top_origin.spec()},
        {"Merchant Payment Request Origin",
         event_data->payment_request_origin.spec()}};
    if (event_data->currency &&
        base::FeatureList::IsEnabled(features::kWebPaymentsMinimalUI)) {
      data["Currency"] = *event_data->currency;
    }
    AddMethodDataToMap(event_data->method_data, &data);
    AddModifiersToMap(event_data->modifiers, &data);
    dev_tools->LogBackgroundServiceEvent(
        registration_id, sw_origin, DevToolsBackgroundService::kPaymentHandler,
        "Can make payment",
        /*instance_id=*/payment_request_id, data);
  }

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(
          payment_request_web_contents_->GetBrowserContext()));
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      partition->GetServiceWorkerContext();

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          &ServiceWorkerCoreThreadEventDispatcher::CanMakePaymentOnCoreThread,
          event_dispatcher_->GetWeakPtr(), registration_id, sw_origin,
          payment_request_id, std::move(dev_tools),
          std::move(service_worker_context), std::move(event_data),
          std::move(callback)));
}

void PaymentAppProviderImpl::AbortPayment(int64_t registration_id,
                                          const url::Origin& sw_origin,
                                          const std::string& payment_request_id,
                                          AbortCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(payment_request_web_contents_);

  scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools =
      GetDevTools(sw_origin);
  if (dev_tools) {
    dev_tools->LogBackgroundServiceEvent(
        registration_id, sw_origin, DevToolsBackgroundService::kPaymentHandler,
        "Abort payment",
        /*instance_id=*/payment_request_id, {});
  }

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(
          payment_request_web_contents_->GetBrowserContext()));
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      partition->GetServiceWorkerContext();

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          &ServiceWorkerCoreThreadEventDispatcher::AbortPaymentOnCoreThread,
          event_dispatcher_->GetWeakPtr(), registration_id, sw_origin,
          payment_request_id, std::move(dev_tools),
          std::move(service_worker_context), std::move(callback)));
}

void PaymentAppProviderImpl::SetOpenedWindow(
    WebContents* payment_handler_web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(payment_handler_web_contents);

  CloseOpenedWindow();
  DCHECK(!payment_handler_window_);

  payment_handler_window_ = std::make_unique<PaymentHandlerWindowObserver>(
      payment_handler_web_contents);
}

void PaymentAppProviderImpl::CloseOpenedWindow() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/1099270): Fix cases where the web contents has already been
  // destroyed without calling this function, e.g. when the bottom sheet UI is
  // closed.
  if (payment_handler_window_ && payment_handler_window_->web_contents()) {
    payment_handler_window_->web_contents()->Close();
  }

  payment_handler_window_.reset();
}

void PaymentAppProviderImpl::OnClosingOpenedWindow(
    PaymentEventResponseType reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&ServiceWorkerCoreThreadEventDispatcher::
                         OnClosingOpenedWindowOnCoreThread,
                     event_dispatcher_->GetWeakPtr(), reason));
}

scoped_refptr<DevToolsBackgroundServicesContextImpl>
PaymentAppProviderImpl::GetDevTools(const url::Origin& sw_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(payment_request_web_contents_);
  auto* storage_partition = BrowserContext::GetStoragePartitionForSite(
      payment_request_web_contents_->GetBrowserContext(), sw_origin.GetURL(),
      /*can_create=*/true);
  if (!storage_partition)
    return nullptr;

  scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools =
      static_cast<DevToolsBackgroundServicesContextImpl*>(
          storage_partition->GetDevToolsBackgroundServicesContext());
  return dev_tools && dev_tools->IsRecording(
                          DevToolsBackgroundService::kPaymentHandler)
             ? dev_tools
             : nullptr;
}

void PaymentAppProviderImpl::StartServiceWorkerForDispatch(
    int64_t registration_id,
    ServiceWorkerCoreThreadEventDispatcher::ServiceWorkerStartCallback
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(
          payment_request_web_contents_->GetBrowserContext()));
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      partition->GetServiceWorkerContext();

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          &ServiceWorkerCoreThreadEventDispatcher::FindRegistrationOnCoreThread,
          event_dispatcher_->GetWeakPtr(), std::move(service_worker_context),
          registration_id, std::move(callback)));
}

void PaymentAppProviderImpl::OnInstallPaymentApp(
    const url::Origin& sw_origin,
    PaymentRequestEventDataPtr event_data,
    RegistrationIdCallback registration_id_callback,
    InvokePaymentAppCallback callback,
    int64_t registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(payment_request_web_contents_);

  if (registration_id >= 0) {
    std::move(registration_id_callback).Run(registration_id);
    InvokePaymentApp(registration_id, sw_origin, std::move(event_data),
                     std::move(callback));
  } else {
    std::move(callback).Run(
        PaymentAppProviderUtil::CreateBlankPaymentHandlerResponse(
            PaymentEventResponseType::PAYMENT_EVENT_BROWSER_ERROR));
  }
}

PaymentAppProviderImpl::PaymentAppProviderImpl(
    WebContents* payment_request_web_contents)
    : payment_request_web_contents_(payment_request_web_contents),
      event_dispatcher_(
          std::make_unique<ServiceWorkerCoreThreadEventDispatcher>(
              payment_request_web_contents_)) {
  event_dispatcher_->set_payment_app_provider(weak_ptr_factory_.GetWeakPtr());
}

PaymentAppProviderImpl::~PaymentAppProviderImpl() {
  BrowserThread::DeleteSoon(ServiceWorkerContext::GetCoreThreadId(), FROM_HERE,
                            std::move(event_dispatcher_));
}

PaymentAppProviderImpl::PaymentHandlerWindowObserver::
    PaymentHandlerWindowObserver(WebContents* payment_handler_web_contents)
    : WebContentsObserver(payment_handler_web_contents) {}
PaymentAppProviderImpl::PaymentHandlerWindowObserver::
    ~PaymentHandlerWindowObserver() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(PaymentAppProviderImpl)
}  // namespace content
