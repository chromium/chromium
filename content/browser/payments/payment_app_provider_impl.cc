// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_provider_impl.h"

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/payments/payment_app_installer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/mojom/base/time.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"
#include "third_party/blink/public/platform/modules/permissions/permission_status.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace content {
namespace {

using ServiceWorkerStartCallback =
    base::OnceCallback<void(scoped_refptr<ServiceWorkerVersion>,
                            blink::ServiceWorkerStatusCode)>;

class RespondWithCallbacks;

// A repository to store invoking payment app callback. It is used to abort
// payment when the opened payment handler window is closed before payment
// response is received or timeout.
// Note that there is only one opened payment handler window per browser
// context.
class InvokePaymentAppCallbackRepository {
 public:
  static InvokePaymentAppCallbackRepository* GetInstance() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    return base::Singleton<InvokePaymentAppCallbackRepository>::get();
  }

  RespondWithCallbacks* GetCallback(BrowserContext* browser_context) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    auto it = invoke_callbacks_.find(browser_context);
    if (it != invoke_callbacks_.end()) {
      return it->second;
    }
    return nullptr;
  }

  void SetCallback(BrowserContext* browser_context,
                   RespondWithCallbacks* callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    invoke_callbacks_[browser_context] = callback;
  }

  void RemoveCallback(BrowserContext* browser_context) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    invoke_callbacks_.erase(browser_context);
  }

 private:
  InvokePaymentAppCallbackRepository() {}
  ~InvokePaymentAppCallbackRepository() {}

  friend struct base::DefaultSingletonTraits<
      InvokePaymentAppCallbackRepository>;

  std::map<BrowserContext*, RespondWithCallbacks*> invoke_callbacks_;
};

// Note that one and only one of the callbacks from this class must/should be
// called.
class RespondWithCallbacks
    : public payments::mojom::PaymentHandlerResponseCallback {
 public:
  RespondWithCallbacks(
      BrowserContext* browser_context,
      ServiceWorkerMetrics::EventType event_type,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      PaymentAppProvider::InvokePaymentAppCallback callback)
      : browser_context_(browser_context),
        event_type_(event_type),
        service_worker_version_(service_worker_version),
        invoke_payment_app_callback_(std::move(callback)),
        binding_(this),
        weak_ptr_factory_(this) {
    request_id_ = service_worker_version->StartRequest(
        event_type, base::BindOnce(&RespondWithCallbacks::OnErrorStatus,
                                   weak_ptr_factory_.GetWeakPtr()));
    InvokePaymentAppCallbackRepository::GetInstance()->SetCallback(
        browser_context, this);
  }

  RespondWithCallbacks(
      BrowserContext* browser_context,
      ServiceWorkerMetrics::EventType event_type,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      PaymentAppProvider::PaymentEventResultCallback callback)
      : browser_context_(browser_context),
        event_type_(event_type),
        service_worker_version_(service_worker_version),
        payment_event_result_callback_(std::move(callback)),
        binding_(this),
        weak_ptr_factory_(this) {
    request_id_ = service_worker_version->StartRequest(
        event_type, base::BindOnce(&RespondWithCallbacks::OnErrorStatus,
                                   weak_ptr_factory_.GetWeakPtr()));
  }

  payments::mojom::PaymentHandlerResponseCallbackPtr
  CreateInterfacePtrAndBind() {
    payments::mojom::PaymentHandlerResponseCallbackPtr callback_proxy;
    binding_.Bind(mojo::MakeRequest(&callback_proxy));
    return callback_proxy;
  }

  void OnResponseForPaymentRequest(
      payments::mojom::PaymentHandlerResponsePtr response,
      base::TimeTicks dispatch_event_time) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    service_worker_version_->FinishRequest(request_id_, false,
                                           std::move(dispatch_event_time));
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(std::move(invoke_payment_app_callback_),
                       std::move(response)));

    ClearCallbackRepositoryAndCloseWindow();
    delete this;
  }

  void OnResponseForCanMakePayment(
      bool can_make_payment,
      base::TimeTicks dispatch_event_time) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    service_worker_version_->FinishRequest(request_id_, false,
                                           std::move(dispatch_event_time));
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(std::move(payment_event_result_callback_),
                       can_make_payment));
    delete this;
  }

  void OnResponseForAbortPayment(bool payment_aborted,
                                 base::TimeTicks dispatch_event_time) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    service_worker_version_->FinishRequest(request_id_, false,
                                           std::move(dispatch_event_time));
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(std::move(payment_event_result_callback_),
                       payment_aborted));

    ClearCallbackRepositoryAndCloseWindow();
    delete this;
  }

  void OnErrorStatus(blink::ServiceWorkerStatusCode service_worker_status) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(service_worker_status != blink::ServiceWorkerStatusCode::kOk);

    if (event_type_ == ServiceWorkerMetrics::EventType::PAYMENT_REQUEST) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(std::move(invoke_payment_app_callback_),
                         payments::mojom::PaymentHandlerResponse::New()));
    } else if (event_type_ ==
                   ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT ||
               event_type_ == ServiceWorkerMetrics::EventType::ABORT_PAYMENT) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(std::move(payment_event_result_callback_), false));
    }

    if (event_type_ == ServiceWorkerMetrics::EventType::PAYMENT_REQUEST ||
        event_type_ == ServiceWorkerMetrics::EventType::ABORT_PAYMENT) {
      ClearCallbackRepositoryAndCloseWindow();
    }
    delete this;
  }

  int request_id() { return request_id_; }

  void AbortPaymentSinceOpennedWindowClosing() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    service_worker_version_->FinishRequest(request_id_, false,
                                           base::TimeTicks::Now());
    OnErrorStatus(blink::ServiceWorkerStatusCode::kErrorAbort);
  }

 private:
  ~RespondWithCallbacks() override {}

  void ClearCallbackRepositoryAndCloseWindow() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    InvokePaymentAppCallbackRepository::GetInstance()->RemoveCallback(
        browser_context_);
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&CloseClientWindowOnUIThread, browser_context_));
  }

  static void CloseClientWindowOnUIThread(BrowserContext* browser_context) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    PaymentAppProvider::GetInstance()->CloseOpenedWindow(browser_context);
  }

  int request_id_;
  BrowserContext* browser_context_;
  ServiceWorkerMetrics::EventType event_type_;
  scoped_refptr<ServiceWorkerVersion> service_worker_version_;
  PaymentAppProvider::InvokePaymentAppCallback invoke_payment_app_callback_;
  PaymentAppProvider::PaymentEventResultCallback payment_event_result_callback_;
  mojo::Binding<payments::mojom::PaymentHandlerResponseCallback> binding_;

  base::WeakPtrFactory<RespondWithCallbacks> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RespondWithCallbacks);
};

void DidGetAllPaymentAppsOnIO(
    PaymentAppProvider::GetAllPaymentAppsCallback callback,
    PaymentAppProvider::PaymentApps apps) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(std::move(callback), std::move(apps)));
}

void GetAllPaymentAppsOnIO(
    scoped_refptr<PaymentAppContextImpl> payment_app_context,
    PaymentAppProvider::GetAllPaymentAppsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  payment_app_context->payment_app_database()->ReadAllPaymentApps(
      base::BindOnce(&DidGetAllPaymentAppsOnIO, std::move(callback)));
}

void DispatchAbortPaymentEvent(
    BrowserContext* browser_context,
    PaymentAppProvider::PaymentEventResultCallback callback,
    scoped_refptr<ServiceWorkerVersion> active_version,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::BindOnce(std::move(callback), false));
    return;
  }

  DCHECK(active_version);

  int event_finish_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT, base::DoNothing());

  // This object self-deletes after either success or error callback is invoked.
  RespondWithCallbacks* invocation_callbacks = new RespondWithCallbacks(
      browser_context, ServiceWorkerMetrics::EventType::ABORT_PAYMENT,
      active_version, std::move(callback));

  active_version->endpoint()->DispatchAbortPaymentEvent(
      invocation_callbacks->CreateInterfacePtrAndBind(),
      active_version->CreateSimpleEventCallback(event_finish_id));
}

void DispatchCanMakePaymentEvent(
    BrowserContext* browser_context,
    payments::mojom::CanMakePaymentEventDataPtr event_data,
    PaymentAppProvider::PaymentEventResultCallback callback,
    scoped_refptr<ServiceWorkerVersion> active_version,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::BindOnce(std::move(callback), false));
    return;
  }

  DCHECK(active_version);

  int event_finish_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT, base::DoNothing());

  // This object self-deletes after either success or error callback is invoked.
  RespondWithCallbacks* invocation_callbacks = new RespondWithCallbacks(
      browser_context, ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT,
      active_version, std::move(callback));

  active_version->endpoint()->DispatchCanMakePaymentEvent(
      std::move(event_data), invocation_callbacks->CreateInterfacePtrAndBind(),
      active_version->CreateSimpleEventCallback(event_finish_id));
}

void DispatchPaymentRequestEvent(
    BrowserContext* browser_context,
    payments::mojom::PaymentRequestEventDataPtr event_data,
    PaymentAppProvider::InvokePaymentAppCallback callback,
    scoped_refptr<ServiceWorkerVersion> active_version,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(std::move(callback),
                       payments::mojom::PaymentHandlerResponse::New()));
    return;
  }

  DCHECK(active_version);

  int event_finish_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::PAYMENT_REQUEST, base::DoNothing());

  // This object self-deletes after either success or error callback is invoked.
  RespondWithCallbacks* invocation_callbacks = new RespondWithCallbacks(
      browser_context, ServiceWorkerMetrics::EventType::PAYMENT_REQUEST,
      active_version, std::move(callback));

  active_version->endpoint()->DispatchPaymentRequestEvent(
      std::move(event_data), invocation_callbacks->CreateInterfacePtrAndBind(),
      active_version->CreateSimpleEventCallback(event_finish_id));
}

void DidFindRegistrationOnIO(
    ServiceWorkerStartCallback callback,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

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

void FindRegistrationOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    int64_t registration_id,
    ServiceWorkerStartCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context->FindReadyRegistrationForIdOnly(
      registration_id,
      base::BindOnce(&DidFindRegistrationOnIO, std::move(callback)));
}

void StartServiceWorkerForDispatch(BrowserContext* browser_context,
                                   int64_t registration_id,
                                   ServiceWorkerStartCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context));
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      partition->GetServiceWorkerContext();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&FindRegistrationOnIO, std::move(service_worker_context),
                     registration_id, std::move(callback)));
}

void OnInstallPaymentApp(payments::mojom::PaymentRequestEventDataPtr event_data,
                         PaymentAppProvider::InvokePaymentAppCallback callback,
                         BrowserContext* browser_context,
                         long registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (registration_id >= 0 && browser_context != nullptr) {
    PaymentAppProvider::GetInstance()->InvokePaymentApp(
        browser_context, registration_id, std::move(event_data),
        std::move(callback));
  } else {
    std::move(callback).Run(payments::mojom::PaymentHandlerResponse::New());
  }
}

void CheckPermissionForPaymentApps(
    BrowserContext* browser_context,
    PaymentAppProvider::GetAllPaymentAppsCallback callback,
    PaymentAppProvider::PaymentApps apps) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PermissionController* permission_controller =
      BrowserContext::GetPermissionController(browser_context);
  DCHECK(permission_controller);

  PaymentAppProvider::PaymentApps permitted_apps;
  for (auto& app : apps) {
    GURL origin = app.second->scope.GetOrigin();
    if (permission_controller->GetPermissionStatus(
            PermissionType::PAYMENT_HANDLER, origin, origin) ==
        blink::mojom::PermissionStatus::GRANTED) {
      permitted_apps[app.first] = std::move(app.second);
    }
  }

  std::move(callback).Run(std::move(permitted_apps));
}

void AbortInvokePaymentApp(BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RespondWithCallbacks* callback =
      InvokePaymentAppCallbackRepository::GetInstance()->GetCallback(
          browser_context);
  if (callback)
    callback->AbortPaymentSinceOpennedWindowClosing();
}

}  // namespace

// static
PaymentAppProvider* PaymentAppProvider::GetInstance() {
  return PaymentAppProviderImpl::GetInstance();
}

// static
PaymentAppProviderImpl* PaymentAppProviderImpl::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::Singleton<PaymentAppProviderImpl>::get();
}

void PaymentAppProviderImpl::GetAllPaymentApps(
    BrowserContext* browser_context,
    GetAllPaymentAppsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context));
  scoped_refptr<PaymentAppContextImpl> payment_app_context =
      partition->GetPaymentAppContext();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&GetAllPaymentAppsOnIO, payment_app_context,
                     base::BindOnce(&CheckPermissionForPaymentApps,
                                    browser_context, std::move(callback))));
}

void PaymentAppProviderImpl::InvokePaymentApp(
    BrowserContext* browser_context,
    int64_t registration_id,
    payments::mojom::PaymentRequestEventDataPtr event_data,
    InvokePaymentAppCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StartServiceWorkerForDispatch(
      browser_context, registration_id,
      base::BindOnce(&DispatchPaymentRequestEvent, browser_context,
                     std::move(event_data), std::move(callback)));
}

void PaymentAppProviderImpl::InstallAndInvokePaymentApp(
    WebContents* web_contents,
    payments::mojom::PaymentRequestEventDataPtr event_data,
    const std::string& app_name,
    const SkBitmap& app_icon,
    const std::string& sw_js_url,
    const std::string& sw_scope,
    bool sw_use_cache,
    const std::string& method,
    InvokePaymentAppCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(base::IsStringUTF8(sw_js_url));
  GURL url = GURL(sw_js_url);
  DCHECK(base::IsStringUTF8(sw_scope));
  GURL scope = GURL(sw_scope);
  if (!url.is_valid() || !scope.is_valid() || method.empty()) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(std::move(callback),
                       payments::mojom::PaymentHandlerResponse::New()));
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
      web_contents, app_name, string_encoded_icon, url, scope, sw_use_cache,
      method,
      base::BindOnce(&OnInstallPaymentApp, std::move(event_data),
                     std::move(callback)));
}

void PaymentAppProviderImpl::CanMakePayment(
    BrowserContext* browser_context,
    int64_t registration_id,
    payments::mojom::CanMakePaymentEventDataPtr event_data,
    PaymentEventResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StartServiceWorkerForDispatch(
      browser_context, registration_id,
      base::BindOnce(&DispatchCanMakePaymentEvent, browser_context,
                     std::move(event_data), std::move(callback)));
}

void PaymentAppProviderImpl::AbortPayment(BrowserContext* browser_context,
                                          int64_t registration_id,
                                          PaymentEventResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StartServiceWorkerForDispatch(
      browser_context, registration_id,
      base::BindOnce(&DispatchAbortPaymentEvent, browser_context,
                     std::move(callback)));
}

void PaymentAppProviderImpl::SetOpenedWindow(WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CloseOpenedWindow(web_contents->GetBrowserContext());

  payment_handler_windows_[web_contents->GetBrowserContext()] =
      std::make_unique<PaymentHandlerWindowObserver>(web_contents);
}

void PaymentAppProviderImpl::CloseOpenedWindow(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = payment_handler_windows_.find(browser_context);
  if (it != payment_handler_windows_.end()) {
    if (it->second->web_contents() != nullptr) {
      it->second->web_contents()->Close();
    }
    payment_handler_windows_.erase(it);
  }
}

void PaymentAppProviderImpl::OnClosingOpenedWindow(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AbortInvokePaymentApp, browser_context));
}

bool PaymentAppProviderImpl::IsValidInstallablePaymentApp(
    const GURL& manifest_url,
    const GURL& sw_js_url,
    const GURL& sw_scope,
    std::string* error_message) {
  DCHECK(manifest_url.is_valid() && sw_js_url.is_valid() &&
         sw_scope.is_valid());

  // Scope will be checked against service worker js url when registering, but
  // we check it here earlier to avoid presenting unusable payment handlers.
  if (!ServiceWorkerUtils::IsPathRestrictionSatisfiedWithoutHeader(
          sw_scope, sw_js_url, error_message)) {
    return false;
  }

  // TODO(crbug.com/855312): Unify duplicated code between here and
  // ServiceWorkerProviderHost::IsValidRegisterMessage.
  std::vector<GURL> urls = {manifest_url, sw_js_url, sw_scope};
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
    *error_message =
        "Origins are not matching, or some origins cannot access service "
        "worker "
        "(manifest:" +
        manifest_url.spec() + " scope:" + sw_scope.spec() +
        " sw:" + sw_js_url.spec() + ")";
    return false;
  }

  return true;
}

PaymentAppProviderImpl::PaymentAppProviderImpl() = default;

PaymentAppProviderImpl::~PaymentAppProviderImpl() = default;

PaymentAppProviderImpl::PaymentHandlerWindowObserver::
    PaymentHandlerWindowObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents) {}
PaymentAppProviderImpl::PaymentHandlerWindowObserver::
    ~PaymentHandlerWindowObserver() = default;

}  // namespace content
