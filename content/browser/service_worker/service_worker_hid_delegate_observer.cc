// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_hid_delegate_observer.h"

#include <cstdint>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/hid/hid_service.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace content {

namespace {

bool HasDevicePermission(HidDelegate* delegate,
                         BrowserContext* browser_context,
                         const blink::StorageKey& key,
                         const device::mojom::HidDeviceInfo& device_info) {
  return delegate && delegate->HasDevicePermission(
                         browser_context, /*render_frame_host=*/nullptr,
                         key.origin(), device_info);
}

}  // namespace

ServiceWorkerHidDelegateObserver::ServiceWorkerHidDelegateObserver(
    ServiceWorkerContextCore* context)
    : ServiceWorkerDeviceDelegateObserver(context) {}

ServiceWorkerHidDelegateObserver::~ServiceWorkerHidDelegateObserver() = default;

void ServiceWorkerHidDelegateObserver::OnDeviceAdded(
    const device::mojom::HidDeviceInfo& device_info) {
  DispatchHidDeviceEventToWorkers(
      device_info,
      base::BindRepeating(
          [](const device::mojom::HidDeviceInfo& device_info,
             HidService* service) { service->OnDeviceAdded(device_info); }));
}

void ServiceWorkerHidDelegateObserver::OnDeviceRemoved(
    const device::mojom::HidDeviceInfo& device_info) {
  DispatchHidDeviceEventToWorkers(
      device_info,
      base::BindRepeating(
          [](const device::mojom::HidDeviceInfo& device_info,
             HidService* service) { service->OnDeviceRemoved(device_info); }));
}

void ServiceWorkerHidDelegateObserver::OnDeviceChanged(
    const device::mojom::HidDeviceInfo& device_info) {
  DispatchHidDeviceEventToWorkers(
      device_info,
      base::BindRepeating(
          [](const device::mojom::HidDeviceInfo& device_info,
             HidService* service) { service->OnDeviceChanged(device_info); }));
}

void ServiceWorkerHidDelegateObserver::OnHidManagerConnectionError() {
  for (auto const& [id, info] : registration_id_map()) {
    for (auto const& hid_service : GetHidServices(id)) {
      if (hid_service) {
        hid_service->OnHidManagerConnectionError();
      }
    }
  }
}

void ServiceWorkerHidDelegateObserver::OnPermissionRevoked(
    const url::Origin& origin) {
  for (auto const& [id, info] : registration_id_map()) {
    for (auto const& hid_service : GetHidServices(id)) {
      if (hid_service) {
        hid_service->OnPermissionRevoked(origin);
      }
    }
  }
}

void ServiceWorkerHidDelegateObserver::RegisterHidService(
    int64_t registration_id,
    base::WeakPtr<HidService> hid_service) {
  Register(registration_id);
  // Multiple HidService instances can exist for the same `registration_id`
  // (e.g., an installing version evaluating top-level `navigator.hid` listeners
  // creates a new instance while the active version is still running). Prune
  // invalidated weak pointers and append `hid_service` so all live instances
  // are notified on permission revocation.
  auto& services = hid_services_[registration_id];
  std::erase_if(services, [](const auto& service) { return !service; });
  services.push_back(std::move(hid_service));
}

void ServiceWorkerHidDelegateObserver::RegistrationAdded(
    int64_t registration_id) {
  if (registration_id_map().size() == 1) {
    HidDelegate* delegate = GetContentClient()->browser()->GetHidDelegate();
    if (delegate) {
      hid_delegate_observation.Observe(delegate);
    }
  }
}

void ServiceWorkerHidDelegateObserver::RegistrationRemoved(
    int64_t registration_id) {
  for (auto const& hid_service : GetHidServices(registration_id)) {
    if (hid_service) {
      hid_service->OnHidManagerConnectionError();
    }
  }
  hid_services_.erase(registration_id);
  if (registration_id_map().empty()) {
    hid_delegate_observation.Reset();
  }
}

void ServiceWorkerHidDelegateObserver::DispatchHidDeviceEventToWorkers(
    const device::mojom::HidDeviceInfo& device_info,
    HidServiceDeviceEventCallback callback) {
  for (auto const& [id, info] : registration_id_map()) {
    // Forward the event to all currently running (kRunning) HidService
    // instances so that open device connections are cleaned up immediately
    // (e.g. on device removal), even if no JS event listeners were registered.
    // Track whether the event was delivered to an already-running ACTIVATED or
    // ACTIVATING version whose client is ready so we know if we need to wake it
    // up below.
    bool delivered_to_active_version = false;
    for (auto const& hid_service : GetHidServices(id)) {
      if (!hid_service) {
        continue;
      }
      auto version = hid_service->service_worker_version();
      if (version &&
          version->running_status() == blink::EmbeddedWorkerStatus::kRunning) {
        auto status = version->status();
        callback.Run(device_info, hid_service.get());
        // Mark as delivered only if the active version (ACTIVATING or
        // ACTIVATED) already has a registered client. If `clients()` is empty
        // (RegisterClient Mojo call still in flight), leave this false so
        // DispatchEventToWorker queues a pending callback rather than losing
        // the event. Checking ACTIVATING prevents double-notifying when it
        // transitions to ACTIVATED in DispatchEventToWorker.
        if ((status == ServiceWorkerVersion::ACTIVATED ||
             status == ServiceWorkerVersion::ACTIVATING) &&
            !hid_service->clients().empty()) {
          delivered_to_active_version = true;
        }
      }
    }
    if (delivered_to_active_version) {
      continue;
    }

    // Check `has_event_handlers` after notifying running services above, since
    // running workers must clean up open device connections (e.g. on device
    // removal) even without JS listeners, whereas a stopped/sleeping ACTIVATED
    // worker should only be woken up if it registered JS event listeners.
    if (!info.has_event_handlers) {
      continue;
    }

    // Avoid waking up the worker if eventually the device event won't be
    // delivered for the HidService.
    auto* delegate = GetContentClient()->browser()->GetHidDelegate();
    if (!HasDevicePermission(delegate, GetBrowserContext(), info.key,
                             device_info)) {
      continue;
    }
    auto filtered_device_info = device_info.Clone();
    HidService::RemoveProtectedReports(
        *filtered_device_info,
        delegate->IsKnownSecurityKey(GetBrowserContext(), device_info),
        delegate->IsFidoAllowedForOrigin(GetBrowserContext(),
                                         info.key.origin()));
    if (filtered_device_info->collections.empty()) {
      continue;
    }

    DispatchEventToWorker(
        id, base::BindOnce(&ServiceWorkerHidDelegateObserver::WorkerStarted,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(filtered_device_info), callback));
  }
}

void ServiceWorkerHidDelegateObserver::WorkerStarted(
    device::mojom::HidDeviceInfoPtr device_info,
    HidServiceDeviceEventCallback callback,
    scoped_refptr<ServiceWorkerVersion> version,
    blink::ServiceWorkerStatusCode service_worker_status) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (!version ||
      service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    return;
  }

  auto registration_id = version->registration_id();
  // Deliver the event only to HidService instances belonging to the woken
  // `version` (ignoring services from other versions of the same registration)
  // that already have a registered render-side HidManagerClient.
  bool has_ready_service = false;
  for (auto const& hid_service : GetHidServices(registration_id)) {
    if (hid_service &&
        hid_service->service_worker_version().get() == version.get() &&
        !hid_service->clients().empty()) {
      callback.Run(*device_info, hid_service.get());
      has_ready_service = true;
    }
  }
  // Even when `version` is in the running state, its HidService may not yet be
  // created or its render-side HidManagerClient may not yet be registered
  // (since the worker enters kRunning after script evaluation while the Mojo
  // requests to create HidService / register HidManagerClient are still in
  // flight). Store the callback to be run once `version`'s HidService client is
  // registered.
  if (!has_ready_service) {
    AddPendingCallback(
        version.get(),
        base::BindOnce(&ServiceWorkerHidDelegateObserver::WorkerStarted,
                       base::Unretained(this), std::move(device_info),
                       std::move(callback), version, service_worker_status));
  }
}

std::vector<base::WeakPtr<HidService>>
ServiceWorkerHidDelegateObserver::GetHidServices(int64_t registration_id) {
  auto it = hid_services_.find(registration_id);
  if (it == hid_services_.end()) {
    return {};
  }
  std::erase_if(it->second, [](const auto& service) { return !service; });
  if (it->second.empty()) {
    hid_services_.erase(it);
    return {};
  }
  return it->second;
}

}  // namespace content
