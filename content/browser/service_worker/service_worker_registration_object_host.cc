// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration_object_host.h"

#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/service_worker/service_worker_security_utils.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

namespace {

// Returns an object info to send over Mojo. The info must be sent immediately.
// See ServiceWorkerObjectHost::CreateCompleteObjectInfoToSend() for details.
blink::mojom::ServiceWorkerObjectInfoPtr CreateCompleteObjectInfoToSend(
    ServiceWorkerContainerHost* container_host,
    ServiceWorkerVersion* version) {
  base::WeakPtr<ServiceWorkerObjectHost> service_worker_object_host =
      container_host->version_object_manager().GetOrCreateHost(version);
  if (!service_worker_object_host)
    return nullptr;
  return service_worker_object_host->CreateCompleteObjectInfoToSend();
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
  DCHECK(registration_);

  // 2. Let |newest_worker| be the result of running Get Newest Worker algorithm
  // passing |registration| as its argument.
  ServiceWorkerVersion* newest_worker = registration_->GetNewestVersion();

  if (!CanServeRegistrationObjectHostMethods(
          &callback,
          registration_->ComposeUpdateErrorMessagePrefix(newest_worker))) {
    return;
  }

  // 3. If |newest_worker| is null, return a promise rejected with an
  // "InvalidStateError" DOMException and abort these steps.
  if (!newest_worker) {
    // This can happen if update() is called during initial script evaluation.
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kState,
        registration_->ComposeUpdateErrorMessagePrefix(nullptr) +
            ServiceWorkerConsts::kInvalidStateErrorMessage);
    return;
  }

  // 4. If the context object’s relevant settings object’s global object
  // globalObject is a ServiceWorkerGlobalScope object, and globalObject’s
  // associated service worker's state is installing, return a promise rejected
  // with an "InvalidStateError" DOMException and abort these steps.
  container_host_->Update(registration_,
                          std::move(outside_fetch_client_settings_object),
                          std::move(callback));
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
                          std::nullopt,
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
                          std::nullopt);
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
                          std::nullopt);
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
                          std::nullopt);
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
  container_host_->registration_object_manager().RemoveHost(
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
    SCOPED_CRASH_KEY_STRING256("SWROH_CSROHM", "host_url",
                               container_host_->url().spec());
    SCOPED_CRASH_KEY_STRING256("SWROH_CSROHM", "reg_scope",
                               registration_->scope().spec());
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

}  // namespace content
