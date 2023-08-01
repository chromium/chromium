// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_hid_delegate_observer.h"

#include <cstdint>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/hid/hid_service.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"

namespace content {

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
    if (!info.has_event_listener) {
      continue;
    }
    // Forward it to HidService if the service worker is running, HidService is
    // available, and it has clients registered.
    auto* hid_service = GetHidService(id);
    if (hid_service) {
      auto version = hid_service->service_worker_version();
      if (version &&
          version->running_status() == EmbeddedWorkerStatus::RUNNING) {
        callback.Run(device_info, hid_service);
        continue;
      }
    }
    // TODO:(crbug.com/1446487): Wake up SW for HID events.
  }
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
