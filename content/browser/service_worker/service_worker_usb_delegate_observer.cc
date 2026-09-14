// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_usb_delegate_observer.h"

#include <cstdint>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/usb/web_usb_service_impl.h"
#include "content/public/common/content_client.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"

namespace content {

namespace {

bool HasDevicePermission(UsbDelegate* delegate,
                         BrowserContext* browser_context,
                         const blink::StorageKey& key,
                         const device::mojom::UsbDeviceInfo& device_info) {
  return delegate && delegate->HasDevicePermission(browser_context,
                                                   /*frame=*/nullptr,
                                                   key.origin(), device_info);
}

}  // namespace

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
    for (auto const& usb_service : GetUsbServices(id)) {
      if (usb_service) {
        usb_service->OnDeviceManagerConnectionError();
      }
    }
  }
}

void ServiceWorkerUsbDelegateObserver::OnPermissionRevoked(
    const url::Origin& origin) {
  for (auto const& [id, info] : registration_id_map()) {
    for (auto const& usb_service : GetUsbServices(id)) {
      if (usb_service) {
        usb_service->OnPermissionRevoked(origin);
      }
    }
  }
}

void ServiceWorkerUsbDelegateObserver::RegisterUsbService(
    int64_t registration_id,
    base::WeakPtr<WebUsbServiceImpl> usb_service) {
  Register(registration_id);
  // Multiple WebUsbServiceImpl instances can exist for the same
  // `registration_id` (e.g., an installing version evaluating top-level
  // `navigator.usb` listeners creates a new instance while the active version
  // is still running). Prune invalidated weak pointers and append `usb_service`
  // so all live instances are notified on permission revocation.
  auto& services = usb_services_[registration_id];
  std::erase_if(services, [](const auto& service) { return !service; });
  services.push_back(std::move(usb_service));
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
  for (auto const& usb_service : GetUsbServices(registration_id)) {
    if (usb_service) {
      usb_service->OnDeviceManagerConnectionError();
    }
  }
  usb_services_.erase(registration_id);
  if (registration_id_map().empty()) {
    usb_delegate_observation.Reset();
  }
}

void ServiceWorkerUsbDelegateObserver::DispatchUsbDeviceEventToWorkers(
    const device::mojom::UsbDeviceInfo& device_info,
    UsbServiceDeviceEventCallback callback) {
  for (auto const& [id, info] : registration_id_map()) {
    // Forward the event to all currently running (kRunning) WebUsbServiceImpl
    // instances so that open device connections are cleaned up immediately
    // (e.g. on device removal), even if no JS event listeners were registered.
    // Track whether the event was delivered to an already-running ACTIVATED or
    // ACTIVATING version whose client is ready so we know if we need to wake it
    // up below.
    bool delivered_to_active_version = false;
    for (auto const& usb_service : GetUsbServices(id)) {
      if (!usb_service) {
        continue;
      }
      auto version = usb_service->service_worker_version();
      if (version &&
          version->running_status() == blink::EmbeddedWorkerStatus::kRunning) {
        auto status = version->status();
        callback.Run(device_info, usb_service.get());
        // Mark as delivered only if the active version (ACTIVATING or
        // ACTIVATED) already has a registered client. If `clients()` is empty
        // (SetClient Mojo call still in flight), leave this false so
        // DispatchEventToWorker queues a pending callback rather than losing
        // the event. Checking ACTIVATING prevents double-notifying when it
        // transitions to ACTIVATED in DispatchEventToWorker.
        if ((status == ServiceWorkerVersion::ACTIVATED ||
             status == ServiceWorkerVersion::ACTIVATING) &&
            !usb_service->clients().empty()) {
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
    // delivered for the WebUsbService.
    if (!HasDevicePermission(GetContentClient()->browser()->GetUsbDelegate(),
                             GetBrowserContext(), info.key, device_info)) {
      continue;
    }

    DispatchEventToWorker(
        id, base::BindOnce(&ServiceWorkerUsbDelegateObserver::WorkerStarted,
                           weak_ptr_factory_.GetWeakPtr(), device_info.Clone(),
                           callback));
  }
}

void ServiceWorkerUsbDelegateObserver::WorkerStarted(
    device::mojom::UsbDeviceInfoPtr device_info,
    UsbServiceDeviceEventCallback callback,
    scoped_refptr<ServiceWorkerVersion> version,
    blink::ServiceWorkerStatusCode service_worker_status) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (!version ||
      service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    return;
  }

  auto registration_id = version->registration_id();
  // Deliver the event only to WebUsbServiceImpl instances belonging to the
  // woken `version` (ignoring services from other versions of the same
  // registration) that already have a registered render-side
  // DeviceManagerClient.
  bool has_ready_service = false;
  for (auto const& usb_service : GetUsbServices(registration_id)) {
    if (usb_service &&
        usb_service->service_worker_version().get() == version.get() &&
        !usb_service->clients().empty()) {
      callback.Run(*device_info, usb_service.get());
      has_ready_service = true;
    }
  }
  // Even when `version` is in the running state, its WebUsbServiceImpl may not
  // yet be created or its render-side DeviceManagerClient may not yet be
  // registered (since the worker enters kRunning after script evaluation while
  // the Mojo requests to create WebUsbServiceImpl / register
  // DeviceManagerClient are still in flight). Store the callback to be run once
  // `version`'s WebUsbServiceImpl client is registered.
  if (!has_ready_service) {
    AddPendingCallback(
        version.get(),
        base::BindOnce(&ServiceWorkerUsbDelegateObserver::WorkerStarted,
                       base::Unretained(this), std::move(device_info),
                       std::move(callback), version, service_worker_status));
  }
}

std::vector<base::WeakPtr<WebUsbServiceImpl>>
ServiceWorkerUsbDelegateObserver::GetUsbServices(int64_t registration_id) {
  auto it = usb_services_.find(registration_id);
  if (it == usb_services_.end()) {
    return {};
  }
  std::erase_if(it->second, [](const auto& service) { return !service; });
  if (it->second.empty()) {
    usb_services_.erase(it);
    return {};
  }
  return it->second;
}

}  // namespace content
