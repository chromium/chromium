// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_provider_impl.h"

#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
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
#include "third_party/blink/public/common/storage_key/storage_key.h"
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

std::string EncodeIcon(const SkBitmap& app_icon) {
  if (app_icon.empty())
    return "";

  gfx::Image decoded_image = gfx::Image::CreateFrom1xBitmap(app_icon);
  scoped_refptr<base::RefCountedMemory> raw_data = decoded_image.As1xPNGBytes();
  return base::Base64Encode(*raw_data);
}

void CheckRegistrationSuccess(base::OnceCallback<void(bool success)> callback,
                              int64_t registration_id) {
  std::move(callback).Run(/*success=*/registration_id >= 0);
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

  if (DevToolsBackgroundServicesContextImpl* dev_tools =
          GetDevTools(sw_origin)) {
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
        registration_id, blink::StorageKey::CreateFirstParty(sw_origin),
        DevToolsBackgroundService::kPaymentHandler, "Payment request",
        /*instance_id=*/event_data->payment_request_id, data);
  }

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      payment_request_web_contents_->GetBrowserContext()
          ->GetDefaultStoragePartition());
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      partition->GetServiceWorkerContext();

  event_dispatcher_->InvokePayment(registration_id, sw_origin,
                                   std::move(service_worker_context),
                                   std::move(event_data), std::move(callback));
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
                PaymentEventResponseType::PAYMENT_HANDLER_INSTALL_FAILED)));
    return;
  }

  url::Origin sw_origin = url::Origin::Create(sw_scope);
  if (DevToolsBackgroundServicesContextImpl* dev_tools =
          GetDevTools(sw_origin)) {
    std::map<std::string, std::string> data = {
        {"Merchant Top Origin", event_data->top_origin.spec()},
        {"Merchant Payment Request Origin",
         event_data->payment_request_origin.spec()},
        {"Method Name", method},
        {"Payment Handler Name", app_name},
        {"Service Worker JavaScript File URL", sw_js_url.spec()},
        {"Service Worker Scope", sw_scope.spec()},
        {"Service Worker Uses Cache", sw_use_cache ? "true" : "false"},
    };
    dev_tools->LogBackgroundServiceEvent(
        /*service_worker_registration_id=*/-1,
        blink::StorageKey::CreateFirstParty(sw_origin),
        DevToolsBackgroundService::kPaymentHandler, "Install payment handler",
        /*instance_id=*/event_data->payment_request_id, data);
  }

  PaymentAppInstaller::Install(
      payment_request_web_contents_, app_name, EncodeIcon(app_icon), sw_js_url,
      sw_scope, sw_use_cache, method, supported_delegations,
      base::BindOnce(&PaymentAppProviderImpl::OnInstallPaymentApp,
                     weak_ptr_factory_.GetWeakPtr(), sw_origin,
                     std::move(event_data), std::move(registration_id_callback),
                     std::move(callback)));
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
      payment_request_web_contents_->GetBrowserContext()
          ->GetDefaultStoragePartition());
  scoped_refptr<PaymentAppContextImpl> payment_app_context =
      partition->GetPaymentAppContext();

  payment_app_context->payment_app_database()
      ->SetPaymentAppInfoForRegisteredServiceWorker(
          registration_id, instrument_key, name, string_encoded_icon,
          method_name, supported_delegations, std::move(callback));
}

void PaymentAppProviderImpl::CanMakePayment(
    int64_t registration_id,
    const url::Origin& sw_origin,
    const std::string& payment_request_id,
    CanMakePaymentEventDataPtr event_data,
    CanMakePaymentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(payment_request_web_contents_);

  if (DevToolsBackgroundServicesContextImpl* dev_tools =
          GetDevTools(sw_origin)) {
    std::map<std::string, std::string> data = {
        {"Merchant Top Origin", event_data->top_origin.spec()},
        {"Merchant Payment Request Origin",
         event_data->payment_request_origin.spec()}};
    AddMethodDataToMap(event_data->method_data, &data);
    AddModifiersToMap(event_data->modifiers, &data);
    dev_tools->LogBackgroundServiceEvent(
        registration_id, blink::StorageKey::CreateFirstParty(sw_origin),
        DevToolsBackgroundService::kPaymentHandler, "Can make payment",
        /*instance_id=*/payment_request_id, data);
  }

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      payment_request_web_contents_->GetBrowserContext()
          ->GetDefaultStoragePartition());
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      partition->GetServiceWorkerContext();

  event_dispatcher_->CanMakePayment(registration_id, sw_origin,
                                    payment_request_id,
                                    std::move(service_worker_context),
                                    std::move(event_data), std::move(callback));
}

void PaymentAppProviderImpl::AbortPayment(int64_t registration_id,
                                          const url::Origin& sw_origin,
                                          const std::string& payment_request_id,
                                          AbortCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(payment_request_web_contents_);

  if (DevToolsBackgroundServicesContextImpl* dev_tools =
          GetDevTools(sw_origin)) {
    dev_tools->LogBackgroundServiceEvent(
        registration_id, blink::StorageKey::CreateFirstParty(sw_origin),
        DevToolsBackgroundService::kPaymentHandler, "Abort payment",
        /*instance_id=*/payment_request_id, {});
  }

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      payment_request_web_contents_->GetBrowserContext()
          ->GetDefaultStoragePartition());
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      partition->GetServiceWorkerContext();

  event_dispatcher_->AbortPayment(
      registration_id, sw_origin, payment_request_id,
      std::move(service_worker_context), std::move(callback));
}

void PaymentAppProviderImpl::SetOpenedWindow(
    WebContents* payment_handler_web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(payment_handler_web_contents);

  CloseOpenedWindow();
  DCHECK(!payment_handler_window_);

  payment_handler_window_ = payment_handler_web_contents->GetWeakPtr();
}

void PaymentAppProviderImpl::CloseOpenedWindow() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (payment_handler_window_)
    payment_handler_window_->Close();

  payment_handler_window_.reset();
}

void PaymentAppProviderImpl::OnClosingOpenedWindow(
    PaymentEventResponseType reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  event_dispatcher_->OnClosingOpenedWindow(reason);
}

void PaymentAppProviderImpl::InstallPaymentAppForTesting(
    const SkBitmap& app_icon,
    const GURL& service_worker_javascript_file_url,
    const GURL& service_worker_scope,
    const std::string& payment_method_identifier,
    base::OnceCallback<void(bool success)> callback) {
  CHECK(service_worker_javascript_file_url.is_valid());
  CHECK(service_worker_scope.is_valid());
  CHECK(!payment_method_identifier.empty());

  PaymentAppInstaller::Install(
      payment_request_web_contents_, /*app_name=*/"Test App Name",
      EncodeIcon(app_icon), service_worker_javascript_file_url,
      service_worker_scope, /*use_cache=*/false, payment_method_identifier,
      content::SupportedDelegations(),
      base::BindOnce(&CheckRegistrationSuccess, std::move(callback)));
}

DevToolsBackgroundServicesContextImpl* PaymentAppProviderImpl::GetDevTools(
    const url::Origin& sw_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(payment_request_web_contents_);
  auto* storage_partition =
      payment_request_web_contents_->GetBrowserContext()
          ->GetStoragePartitionForUrl(sw_origin.GetURL(),
                                      /*can_create=*/true);
  if (!storage_partition)
    return nullptr;

  DevToolsBackgroundServicesContextImpl* dev_tools =
      static_cast<DevToolsBackgroundServicesContextImpl*>(
          storage_partition->GetDevToolsBackgroundServicesContext());
  return dev_tools && dev_tools->IsRecording(
                          DevToolsBackgroundService::kPaymentHandler)
             ? dev_tools
             : nullptr;
}

void PaymentAppProviderImpl::StartServiceWorkerForDispatch(
    int64_t registration_id,
    PaymentEventDispatcher::ServiceWorkerStartCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      payment_request_web_contents_->GetBrowserContext()
          ->GetDefaultStoragePartition());
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      partition->GetServiceWorkerContext();

  event_dispatcher_->FindRegistration(std::move(service_worker_context),
                                      registration_id, std::move(callback));
}

void PaymentAppProviderImpl::OnInstallPaymentApp(
    const url::Origin& sw_origin,
    PaymentRequestEventDataPtr event_data,
    RegistrationIdCallback registration_id_callback,
    InvokePaymentAppCallback callback,
    int64_t registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(payment_request_web_contents_);

  if (DevToolsBackgroundServicesContextImpl* dev_tools =
          GetDevTools(sw_origin)) {
    std::map<std::string, std::string> data = {
        {"Payment Handler Install Success",
         registration_id >= 0 ? "true" : "false"},
    };
    dev_tools->LogBackgroundServiceEvent(
        registration_id, blink::StorageKey::CreateFirstParty(sw_origin),
        DevToolsBackgroundService::kPaymentHandler,
        "Install payment handler result",
        /*instance_id=*/event_data->payment_request_id, data);
  }

  if (registration_id >= 0) {
    std::move(registration_id_callback).Run(registration_id);
    InvokePaymentApp(registration_id, sw_origin, std::move(event_data),
                     std::move(callback));
  } else {
    std::move(callback).Run(
        PaymentAppProviderUtil::CreateBlankPaymentHandlerResponse(
            PaymentEventResponseType::PAYMENT_HANDLER_INSTALL_FAILED));
  }
}

PaymentAppProviderImpl::PaymentAppProviderImpl(
    WebContents* payment_request_web_contents)
    : WebContentsUserData<PaymentAppProviderImpl>(
          *payment_request_web_contents),
      payment_request_web_contents_(payment_request_web_contents),
      event_dispatcher_(std::make_unique<PaymentEventDispatcher>()) {
  event_dispatcher_->set_payment_app_provider(weak_ptr_factory_.GetWeakPtr());
}

PaymentAppProviderImpl::~PaymentAppProviderImpl() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(PaymentAppProviderImpl);
}  // namespace content
