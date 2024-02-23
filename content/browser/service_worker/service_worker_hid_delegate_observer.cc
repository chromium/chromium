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
    auto* hid_service = GetHidService(id);
    if (hid_service) {
      hid_service->OnHidManagerConnectionError();
    }
  }
}

void ServiceWorkerHidDelegateObserver::OnPermissionRevoked(
    const url::Origin& origin) {
  for (auto const& [id, info] : registration_id_map()) {
    auto* hid_service = GetHidService(id);
    if (hid_service) {
      hid_service->OnPermissionRevoked(origin);
    }
  }
}

void ServiceWorkerHidDelegateObserver::RegisterHidService(
    int64_t registration_id,
    base::WeakPtr<HidService> hid_service) {
  Register(registration_id);
  // `hid_services_` may already have an entry for `registration_id` in a case
  // where the service worker went to sleep and now is worken up. In that
  // case, the HidService from `hid_services_[registration_id]` is the weak ptr
  // of previous HidService before the service worker went to sleep. We don't
  // care about the previous HidService, so here just overwrite it with
  // `hid_service`, which is the latest one.
  hid_services_[registration_id] = hid_service;
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
  if (registration_id_map().empty()) {
    hid_delegate_observation.Reset();
  }
}

void ServiceWorkerHidDelegateObserver::DispatchHidDeviceEventToWorkers(
    const device::mojom::HidDeviceInfo& device_info,
    HidServiceDeviceEventCallback callback) {
  for (auto const& [id, info] : registration_id_map()) {
    // No need to proceed if the registration doesn't have any event listeners.
    if (!info.has_event_handlers) {
      continue;
    }
    // Forward it to HidService if the service worker is running, HidService is
    // available, and it has clients registered.
    auto* hid_service = GetHidService(id);
    if (hid_service) {
      auto version = hid_service->service_worker_version();
      if (version &&
          version->running_status() == blink::EmbeddedWorkerStatus::kRunning) {
        callback.Run(device_info, hid_service);
        continue;
      }
    }

    // Avoid waking up the worker if eventually the device event won't be
    // delivered for the HidService.
    if (!HasDevicePermission(GetContentClient()->browser()->GetHidDelegate(),
                             GetBrowserContext(), info.key, device_info)) {
      continue;
    }
    auto filtered_device_info = device_info.Clone();
    HidService::RemoveProtectedReports(
        *filtered_device_info,
        GetContentClient()->browser()->GetHidDelegate()->IsFidoAllowedForOrigin(
            GetBrowserContext(), info.key.origin()));
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!version ||
      service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    return;
  }

  auto registration_id = version->registration_id();
  auto* hid_service = GetHidService(registration_id);
  // Even when the service worker is in the running state, the HidService may
  // not be available or the render-side HidManagerClient may not have yet
  // registered. This is because the service worker is set to running state
  // after the script is evaluated, but the inter-process request that creates
  // the HidService or gets the HidManagerClient registered may still be in
  // progress. In order to handle this case, the callback is stored and will be
  // processed when the HidService is ready and the HidManagerClient is
  // registered with the HidService.
  if (!hid_service || hid_service->clients().empty()) {
    AddPendingCallback(
        version.get(),
        base::BindOnce(&ServiceWorkerHidDelegateObserver::WorkerStarted,
                       base::Unretained(this), std::move(device_info),
                       std::move(callback), version, service_worker_status));
    return;
  }
  callback.Run(*device_info, hid_service);
}

HidService* ServiceWorkerHidDelegateObserver::GetHidService(
    int64_t registration_id) {
  auto it = hid_services_.find(registration_id);
  if (it == hid_services_.end()) {
    return nullptr;
  }
  return it->second.get();
}

}  // namespace content
