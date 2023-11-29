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
          version->running_status() == blink::EmbeddedWorkerStatus::kRunning) {
        callback.Run(device_info, usb_service);
        continue;
      }
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!version ||
      service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    return;
  }

  auto registration_id = version->registration_id();
  auto* usb_service = GetUsbService(registration_id);
  // Even when the service worker is in the running state, the WebUsbService may
  // not be available or the render-side DeviceManagerClient may not have yet
  // registered. This is because the service worker is set to running state
  // after the script is evaluated, but the inter-process request that creates
  // the WebUsbService or gets the DeviceManagerClient registered may still be
  // in progress. In order to handle this case, the callback is stored and will
  // be processed when the WebUsbService is ready and the DeviceManagerClient is
  // registered with the WebUsbService.
  if (!usb_service || usb_service->clients().empty()) {
    AddPendingCallback(
        version.get(),
        base::BindOnce(&ServiceWorkerUsbDelegateObserver::WorkerStarted,
                       base::Unretained(this), std::move(device_info),
                       std::move(callback), version, service_worker_status));
    return;
  }
  callback.Run(*device_info, usb_service);
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
