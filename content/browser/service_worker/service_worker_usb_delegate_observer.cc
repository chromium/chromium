// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_usb_delegate_observer.h"

#include <cstdint>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/usb/web_usb_service_impl.h"
#include "content/public/common/content_client.h"

namespace content {

ServiceWorkerUsbDelegateObserver::ServiceWorkerUsbDelegateObserver(
    ServiceWorkerContextCore* context)
    : ServiceWorkerDeviceDelegateObserver(context) {}

ServiceWorkerUsbDelegateObserver::~ServiceWorkerUsbDelegateObserver() = default;

void ServiceWorkerUsbDelegateObserver::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device_info) {
  DispatchUsbDeviceEventToWorkers(
      device_info,
      base::BindRepeating([](const device::mojom::UsbDeviceInfo& device_info,
                             WebUsbServiceImpl* service) {
        service->OnDeviceAdded(device_info);
      }));
}

void ServiceWorkerUsbDelegateObserver::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device_info) {
  DispatchUsbDeviceEventToWorkers(
      device_info,
      base::BindRepeating([](const device::mojom::UsbDeviceInfo& device_info,
                             WebUsbServiceImpl* service) {
        service->OnDeviceRemoved(device_info);
      }));
}

void ServiceWorkerUsbDelegateObserver::OnDeviceManagerConnectionError() {
  for (auto const& [id, info] : registration_id_map()) {
    auto* usb_service = GetUsbService(id);
    if (usb_service) {
      usb_service->OnDeviceManagerConnectionError();
    }
  }
}

void ServiceWorkerUsbDelegateObserver::OnPermissionRevoked(
    const url::Origin& origin) {
  for (auto const& [id, info] : registration_id_map()) {
    auto* usb_service = GetUsbService(id);
    if (usb_service) {
      usb_service->OnPermissionRevoked(origin);
    }
  }
}

void ServiceWorkerUsbDelegateObserver::RegisterUsbService(
    int64_t registration_id,
    base::WeakPtr<WebUsbServiceImpl> usb_service) {
  Register(registration_id);
  // `usb_services_` may already have an entry for `registration_id` in a case
  // where the service worker went to sleep and now is worken up. In that
  // case, the WebUsbServiceImpl from `usb_services_[registration_id]` is the
  // weak ptr of previous WebUsbServiceImpl before the service worker went to
  // sleep. We don't care about the previous WebUsbServiceImpl, so here just
  // overwrite it with `usb_service`, which is the latest one.
  usb_services_[registration_id] = usb_service;
}

void ServiceWorkerUsbDelegateObserver::RegistrationAdded(
    int64_t registration_id) {
  if (registration_id_map().size() == 1) {
    UsbDelegate* delegate = GetContentClient()->browser()->GetUsbDelegate();
    if (delegate) {
      usb_delegate_observation.Observe(delegate);
    }
  }
}

void ServiceWorkerUsbDelegateObserver::RegistrationRemoved(
    int64_t registration_id) {
  if (registration_id_map().empty()) {
    usb_delegate_observation.Reset();
  }
}

void ServiceWorkerUsbDelegateObserver::DispatchUsbDeviceEventToWorkers(
    const device::mojom::UsbDeviceInfo& device_info,
    UsbServiceDeviceEventCallback callback) {
  for (auto const& [id, info] : registration_id_map()) {
    // No need to proceed if the registration doesn't have any event listeners.
    if (!info.has_event_handlers) {
      continue;
    }
    // Forward it to UsbService if the service worker is running, UsbService is
    // available, and it has clients registered.
    auto* usb_service = GetUsbService(id);
    if (usb_service) {
      auto version = usb_service->service_worker_version();
      if (version &&
          version->running_status() == EmbeddedWorkerStatus::RUNNING) {
        callback.Run(device_info, usb_service);
        continue;
      }
    }
  }
}

WebUsbServiceImpl* ServiceWorkerUsbDelegateObserver::GetUsbService(
    int64_t registration_id) {
  auto it = usb_services_.find(registration_id);
  if (it == usb_services_.end()) {
    return nullptr;
  }
  return it->second.get();
}

}  // namespace content
