// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration_object_host.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/service_worker/service_worker_security_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

namespace {

constexpr base::TimeDelta kSelfUpdateDelay = base::Seconds(30);
constexpr base::TimeDelta kMaxSelfUpdateDelay = base::Minutes(3);

// Returns an object info to send over Mojo. The info must be sent immediately.
// See ServiceWorkerObjectHost::CreateCompleteObjectInfoToSend() for details.
blink::mojom::ServiceWorkerObjectInfoPtr CreateCompleteObjectInfoToSend(
    ServiceWorkerContainerHost* container_host,
    ServiceWorkerVersion* version) {
  base::WeakPtr<ServiceWorkerObjectHost> service_worker_object_host =
      container_host->GetOrCreateServiceWorkerObjectHost(version);
  if (!service_worker_object_host)
    return nullptr;
  return service_worker_object_host->CreateCompleteObjectInfoToSend();
}

void ExecuteUpdate(base::WeakPtr<ServiceWorkerContextCore> context,
                   int64_t registration_id,
                   bool force_bypass_cache,
                   bool skip_script_comparison,
                   blink::mojom::FetchClientSettingsObjectPtr
                       outside_fetch_client_settings_object,
                   ServiceWorkerContextCore::UpdateCallback callback,
                   blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    // The delay was already very long and update() is rejected immediately.
    DCHECK_EQ(blink::ServiceWorkerStatusCode::kErrorTimeout, status);
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorTimeout,
                            ServiceWorkerConsts::kUpdateTimeoutErrorMesage,
                            registration_id);
    return;
  }

  if (!context) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort,
                            ServiceWorkerConsts::kShutdownErrorMessage,
                            registration_id);
    return;
  }

  scoped_refptr<ServiceWorkerRegistration> registration =
      context->GetLiveRegistration(registration_id);
  if (!registration) {
    // The service worker is no longer running, so update() won't be rejected.
    // We still run the callback so the caller knows.
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorTimeout,
                            ServiceWorkerConsts::kUpdateTimeoutErrorMesage,
                            registration_id);
    return;
  }

  context->UpdateServiceWorker(
      registration.get(), force_bypass_cache, skip_script_comparison,
      std::move(outside_fetch_client_settings_object), std::move(callback));
}

}  // anonymous namespace

ServiceWorkerRegistrationObjectHost::ServiceWorkerRegistrationObjectHost(
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerContainerHost* container_host,
    scoped_refptr<ServiceWorkerRegistration> registration)
    : container_host_(container_host),
      context_(context),
      registration_(registration) {
  DCHECK(registration_.get());
  DCHECK(container_host_);
  registration_->AddListener(this);
  receivers_.set_disconnect_handler(base::BindRepeating(
      &ServiceWorkerRegistrationObjectHost::OnConnectionError,
      base::Unretained(this)));
}

ServiceWorkerRegistrationObjectHost::~ServiceWorkerRegistrationObjectHost() {
  DCHECK(registration_.get());
  registration_->RemoveListener(this);
}

blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
ServiceWorkerRegistrationObjectHost::CreateObjectInfo() {
  auto info = blink::mojom::ServiceWorkerRegistrationObjectInfo::New();
  info->registration_id = registration_->id();
  info->scope = registration_->scope();
  info->update_via_cache = registration_->update_via_cache();
  receivers_.Add(this, info->host_remote.InitWithNewEndpointAndPassReceiver());

  remote_registration_.reset();
  info->receiver = remote_registration_.BindNewEndpointAndPassReceiver();

  info->installing = CreateCompleteObjectInfoToSend(
      container_host_, registration_->installing_version());
  info->waiting = CreateCompleteObjectInfoToSend(
      container_host_, registration_->waiting_version());
  info->active = CreateCompleteObjectInfoToSend(
      container_host_, registration_->active_version());
  return info;
}

void ServiceWorkerRegistrationObjectHost::OnVersionAttributesChanged(
    ServiceWorkerRegistration* registration,
    blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask) {
  DCHECK_EQ(registration->id(), registration_->id());
  SetServiceWorkerObjects(
      std::move(changed_mask), registration->installing_version(),
      registration->waiting_version(), registration->active_version());
}

void ServiceWorkerRegistrationObjectHost::OnUpdateViaCacheChanged(
    ServiceWorkerRegistration* registration) {
  remote_registration_->SetUpdateViaCache(registration->update_via_cache());
}

void ServiceWorkerRegistrationObjectHost::OnRegistrationFailed(
    ServiceWorkerRegistration* registration) {
  DCHECK_EQ(registration->id(), registration_->id());
  auto changed_mask =
      blink::mojom::ChangedServiceWorkerObjectsMask::New(true, true, true);
  SetServiceWorkerObjects(std::move(changed_mask), nullptr, nullptr, nullptr);
}

void ServiceWorkerRegistrationObjectHost::OnUpdateFound(
    ServiceWorkerRegistration* registration) {
  DCHECK(remote_registration_);
  remote_registration_->UpdateFound();
}

void ServiceWorkerRegistrationObjectHost::Update(
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    UpdateCallback callback) {
  // Run steps according to section 3.2.7:
  // https://w3c.github.io/ServiceWorker/#service-worker-registration-update

  // 1. Let |registration| be the service worker registration.
  ServiceWorkerRegistration* registration = registration_.get();
  DCHECK(registration);

  // 2. Let |newest_worker| be the result of running Get Newest Worker algorithm
  // passing |registration| as its argument.
  ServiceWorkerVersion* newest_worker = registration->GetNewestVersion();

  if (!CanServeRegistrationObjectHostMethods(
          &callback, ComposeUpdateErrorMessagePrefix(newest_worker))) {
    return;
  }

  // 3. If |newest_worker| is null, return a promise rejected with an
  // "InvalidStateError" DOMException and abort these steps.
  if (!newest_worker) {
    // This can happen if update() is called during initial script evaluation.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kState,
                            ComposeUpdateErrorMessagePrefix(nullptr) +
                                ServiceWorkerConsts::kInvalidStateErrorMessage);
    return;
  }

  // 4. If the context object’s relevant settings object’s global object
  // globalObject is a ServiceWorkerGlobalScope object, and globalObject’s
  // associated service worker's state is installing, return a promise rejected
  // with an "InvalidStateError" DOMException and abort these steps.
  ServiceWorkerVersion* version = nullptr;
  if (container_host_->IsContainerForServiceWorker()) {
    version = container_host_->service_worker_host()->version();
    DCHECK(version);
    if (ServiceWorkerVersion::Status::INSTALLING == version->status()) {
      // This can happen if update() is called during execution of the
      // install-event-handler.
      std::move(callback).Run(
          blink::mojom::ServiceWorkerErrorType::kState,
          ComposeUpdateErrorMessagePrefix(version) +
              ServiceWorkerConsts::kInvalidStateErrorMessage);
      return;
    }
  }

  DelayUpdate(
      container_host_->IsContainerForClient(), registration, version,
      base::BindOnce(
          &ExecuteUpdate, context_, registration->id(),
          false /* force_bypass_cache */, false /* skip_script_comparison */,
          std::move(outside_fetch_client_settings_object),
          base::BindOnce(&ServiceWorkerRegistrationObjectHost::UpdateComplete,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void ServiceWorkerRegistrationObjectHost::DelayUpdate(
    bool is_container_for_client,
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version,
    StatusCallback update_function) {
  DCHECK(registration);

  if (is_container_for_client || (version && version->HasControllee())) {
    // Don't delay update() if called by non-workers or by workers with
    // controllees.
    std::move(update_function).Run(blink::ServiceWorkerStatusCode::kOk);
    return;
  }

  base::TimeDelta delay = registration->self_update_delay();
  if (delay > kMaxSelfUpdateDelay) {
    std::move(update_function)
        .Run(blink::ServiceWorkerStatusCode::kErrorTimeout);
    return;
  }

  if (delay < kSelfUpdateDelay) {
    registration->set_self_update_delay(kSelfUpdateDelay);
  } else {
    registration->set_self_update_delay(delay * 2);
  }

  if (delay < base::TimeDelta::Min()) {
    // Only enforce the delay of update() iff |delay| exists.
    std::move(update_function).Run(blink::ServiceWorkerStatusCode::kOk);
    return;
  }

  GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(update_function),
                     blink::ServiceWorkerStatusCode::kOk),
      delay);
}

void ServiceWorkerRegistrationObjectHost::Unregister(
    UnregisterCallback callback) {
  if (!CanServeRegistrationObjectHostMethods(
          &callback,
          std::string(
              ServiceWorkerConsts::kServiceWorkerUnregisterErrorPrefix))) {
    return;
  }
  context_->UnregisterServiceWorker(
      registration_->scope(), registration_->key(),
      /*is_immediate=*/false,
      base::BindOnce(
          &ServiceWorkerRegistrationObjectHost::UnregistrationComplete,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerRegistrationObjectHost::EnableNavigationPreload(
    bool enable,
    EnableNavigationPreloadCallback callback) {
  if (!CanServeRegistrationObjectHostMethods(
          &callback,
          std::string(
              ServiceWorkerConsts::kEnableNavigationPreloadErrorPrefix))) {
    return;
  }

  if (!registration_->active_version()) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kState,
        std::string(ServiceWorkerConsts::kEnableNavigationPreloadErrorPrefix) +
            std::string(ServiceWorkerConsts::kNoActiveWorkerErrorMessage));
    return;
  }

  context_->registry()->UpdateNavigationPreloadEnabled(
      registration_->id(), registration_->key(), enable,
      base::BindOnce(&ServiceWorkerRegistrationObjectHost::
                         DidUpdateNavigationPreloadEnabled,
                     weak_ptr_factory_.GetWeakPtr(), enable,
                     std::move(callback)));
}

void ServiceWorkerRegistrationObjectHost::GetNavigationPreloadState(
    GetNavigationPreloadStateCallback callback) {
  if (!CanServeRegistrationObjectHostMethods(
          &callback,
          std::string(
              ServiceWorkerConsts::kGetNavigationPreloadStateErrorPrefix),
          nullptr)) {
    return;
  }

  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          absl::nullopt,
                          registration_->navigation_preload_state().Clone());
}

void ServiceWorkerRegistrationObjectHost::SetNavigationPreloadHeader(
    const std::string& value,
    SetNavigationPreloadHeaderCallback callback) {
  if (!CanServeRegistrationObjectHostMethods(
          &callback,
          std::string(
              ServiceWorkerConsts::kSetNavigationPreloadHeaderErrorPrefix))) {
    return;
  }

  if (!registration_->active_version()) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kState,
        std::string(
            ServiceWorkerConsts::kSetNavigationPreloadHeaderErrorPrefix) +
            std::string(ServiceWorkerConsts::kNoActiveWorkerErrorMessage));
    return;
  }

  // TODO(falken): Ideally this would match Blink's isValidHTTPHeaderValue.
  // Chrome's check is less restrictive: it allows non-latin1 characters.
  if (!net::HttpUtil::IsValidHeaderValue(value)) {
    receivers_.ReportBadMessage(
        ServiceWorkerConsts::kBadNavigationPreloadHeaderValue);
    return;
  }

  context_->registry()->UpdateNavigationPreloadHeader(
      registration_->id(), registration_->key(), value,
      base::BindOnce(&ServiceWorkerRegistrationObjectHost::
                         DidUpdateNavigationPreloadHeader,
                     weak_ptr_factory_.GetWeakPtr(), value,
                     std::move(callback)));
}

void ServiceWorkerRegistrationObjectHost::UpdateComplete(
    UpdateCallback callback,
    blink::ServiceWorkerStatusCode status,
    const std::string& status_message,
    int64_t registration_id) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::string error_message;
    blink::mojom::ServiceWorkerErrorType error_type;
    GetServiceWorkerErrorTypeForRegistration(status, status_message,
                                             &error_type, &error_message);
    std::move(callback).Run(error_type, ComposeUpdateErrorMessagePrefix(
                                            registration_->GetNewestVersion()) +
                                            error_message);
    return;
  }

  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          absl::nullopt);
}

void ServiceWorkerRegistrationObjectHost::UnregistrationComplete(
    UnregisterCallback callback,
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::string error_message;
    blink::mojom::ServiceWorkerErrorType error_type;
    GetServiceWorkerErrorTypeForRegistration(status, std::string(), &error_type,
                                             &error_message);
    std::move(callback).Run(
        error_type, ServiceWorkerConsts::kServiceWorkerUnregisterErrorPrefix +
                        error_message);
    return;
  }

  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          absl::nullopt);
}

void ServiceWorkerRegistrationObjectHost::DidUpdateNavigationPreloadEnabled(
    bool enable,
    EnableNavigationPreloadCallback callback,
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kUnknown,
        std::string(ServiceWorkerConsts::kEnableNavigationPreloadErrorPrefix) +
            std::string(ServiceWorkerConsts::kDatabaseErrorMessage));
    return;
  }

  if (registration_)
    registration_->EnableNavigationPreload(enable);
  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          absl::nullopt);
}

void ServiceWorkerRegistrationObjectHost::DidUpdateNavigationPreloadHeader(
    const std::string& value,
    SetNavigationPreloadHeaderCallback callback,
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kUnknown,
        std::string(
            ServiceWorkerConsts::kSetNavigationPreloadHeaderErrorPrefix) +
            std::string(ServiceWorkerConsts::kDatabaseErrorMessage));
    return;
  }

  if (registration_)
    registration_->SetNavigationPreloadHeader(value);
  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          absl::nullopt);
}

void ServiceWorkerRegistrationObjectHost::SetServiceWorkerObjects(
    blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask,
    ServiceWorkerVersion* installing_version,
    ServiceWorkerVersion* waiting_version,
    ServiceWorkerVersion* active_version) {
  if (!(changed_mask->installing || changed_mask->waiting ||
        changed_mask->active))
    return;

  blink::mojom::ServiceWorkerObjectInfoPtr installing;
  blink::mojom::ServiceWorkerObjectInfoPtr waiting;
  blink::mojom::ServiceWorkerObjectInfoPtr active;
  if (changed_mask->installing) {
    installing =
        CreateCompleteObjectInfoToSend(container_host_, installing_version);
  }
  if (changed_mask->waiting)
    waiting = CreateCompleteObjectInfoToSend(container_host_, waiting_version);
  if (changed_mask->active)
    active = CreateCompleteObjectInfoToSend(container_host_, active_version);

  DCHECK(remote_registration_);
  remote_registration_->SetServiceWorkerObjects(
      std::move(changed_mask), std::move(installing), std::move(waiting),
      std::move(active));
}

void ServiceWorkerRegistrationObjectHost::OnConnectionError() {
  // If there are still receivers, |this| is still being used.
  if (!receivers_.empty())
    return;
  // Will destroy |this|.
  container_host_->RemoveServiceWorkerRegistrationObjectHost(
      registration()->id());
}

template <typename CallbackType, typename... Args>
bool ServiceWorkerRegistrationObjectHost::CanServeRegistrationObjectHostMethods(
    CallbackType* callback,
    const std::string& error_prefix,
    Args... args) {
  if (!context_) {
    std::move(*callback).Run(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        error_prefix + ServiceWorkerConsts::kShutdownErrorMessage, args...);
    return false;
  }

  // TODO(falken): This check can be removed once crbug.com/439697 is fixed.
  // (Also see crbug.com/776408)
  if (container_host_->url().is_empty()) {
    std::move(*callback).Run(
        blink::mojom::ServiceWorkerErrorType::kSecurity,
        error_prefix + ServiceWorkerConsts::kNoDocumentURLErrorMessage,
        args...);
    return false;
  }

  std::vector<GURL> urls = {container_host_->url(), registration_->scope()};
  if (!service_worker_security_utils::AllOriginsMatchAndCanAccessServiceWorkers(
          urls)) {
    receivers_.ReportBadMessage(
        ServiceWorkerConsts::kBadMessageImproperOrigins);
    return false;
  }

  if (!container_host_->AllowServiceWorker(registration_->scope(), GURL())) {
    std::move(*callback).Run(
        blink::mojom::ServiceWorkerErrorType::kDisabled,
        error_prefix + ServiceWorkerConsts::kUserDeniedPermissionMessage,
        args...);
    return false;
  }

  return true;
}

std::string
ServiceWorkerRegistrationObjectHost::ComposeUpdateErrorMessagePrefix(
    const ServiceWorkerVersion* version_to_update) const {
  DCHECK(registration_);
  const char* script_url = version_to_update
                               ? version_to_update->script_url().spec().c_str()
                               : "Unknown";
  return base::StringPrintf(
      ServiceWorkerConsts::kServiceWorkerUpdateErrorPrefix,
      registration_->scope().spec().c_str(), script_url);
}

}  // namespace content
