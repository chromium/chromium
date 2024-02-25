// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_USB_DELEGATE_OBSERVER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_USB_DELEGATE_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation_traits.h"
#include "content/browser/service_worker/service_worker_device_delegate_observer.h"
#include "content/browser/usb/web_usb_service_impl.h"
#include "content/public/browser/usb_delegate.h"

namespace content {

class ServiceWorkerContextCore;

// ServiceWorkerUsbDelegateObserver acts as a broker between the
// content::WebUsbService and content::UsbDelegate when the WebUsbService is
// created for a service worker. Each ServiceWorkerContextCore has one
// ServiceWorkerUsbDelegateObserver, which is used to track all of the
// ServiceWorkerRegistration objects whose script uses the WebUSB API.
// Furthermore, when a device event happens, the
// ServiceWorkerUsbDelegateObserver's UsbDelegate::Observer methods will be
// invoked and deliver the device events to the service worker that has a device
// event handler registered.
// For more information, please see go/usb-hid-extension-access.
class CONTENT_EXPORT ServiceWorkerUsbDelegateObserver
    : public ServiceWorkerDeviceDelegateObserver,
      public UsbDelegate::Observer {
 public:
  explicit ServiceWorkerUsbDelegateObserver(ServiceWorkerContextCore* context);
  ServiceWorkerUsbDelegateObserver(const ServiceWorkerUsbDelegateObserver&) =
      delete;
  ServiceWorkerUsbDelegateObserver& operator=(
      const ServiceWorkerUsbDelegateObserver&) = delete;
  ~ServiceWorkerUsbDelegateObserver() override;

  // UsbDelegate::Observer:
  void OnDeviceAdded(const device::mojom::UsbDeviceInfo& device_info) override;
  void OnDeviceRemoved(
      const device::mojom::UsbDeviceInfo& device_info) override;
  void OnDeviceManagerConnectionError() override;
  void OnPermissionRevoked(const url::Origin& origin) override;

  // Register the `usb_service` to be the latest WebUsbService for
  // `registraiton_id`.
  void RegisterUsbService(int64_t registration_id,
                          base::WeakPtr<WebUsbServiceImpl> usb_service);

  WebUsbServiceImpl* GetUsbServiceForTesting(int64_t registration_id) {
    return GetUsbService(registration_id);
  }

 private:
  using UsbServiceCallback = base::OnceCallback<void(WebUsbServiceImpl*)>;
  using UsbServiceDeviceEventCallback =
      base::RepeatingCallback<void(const device::mojom::UsbDeviceInfo&,
                                   WebUsbServiceImpl*)>;

  // ServiceWorkerDeviceDelegateObserver:
  void RegistrationAdded(int64_t registration_id) override;
  void RegistrationRemoved(int64_t registration_id) override;

  // Dispatch the device event to all registered service workers.
  void DispatchUsbDeviceEventToWorkers(
      const device::mojom::UsbDeviceInfo& device_info,
      UsbServiceDeviceEventCallback callback);

  // Run `callback` with `device_info` after the worker of `version` is started.
  void WorkerStarted(device::mojom::UsbDeviceInfoPtr device_info,
                     UsbServiceDeviceEventCallback callback,
                     scoped_refptr<ServiceWorkerVersion> version,
                     blink::ServiceWorkerStatusCode service_worker_status);

  // Get UsbService for the `registration_id`. It can be null if no live
  // UsbService for the `registration_id`.
  WebUsbServiceImpl* GetUsbService(int64_t registration_id);

  // The map for registration id to the latest registered UsbService.
  base::flat_map<int64_t, base::WeakPtr<WebUsbServiceImpl>> usb_services_;

  base::ScopedObservation<UsbDelegate, ServiceWorkerUsbDelegateObserver>
      usb_delegate_observation{this};

  base::WeakPtrFactory<ServiceWorkerUsbDelegateObserver> weak_ptr_factory_{
      this};
};

}  // namespace content

namespace base {

template <>
struct ScopedObservationTraits<content::UsbDelegate,
                               content::ServiceWorkerUsbDelegateObserver> {
  static void AddObserver(content::UsbDelegate* source,
                          content::ServiceWorkerUsbDelegateObserver* observer) {
    source->AddObserver(observer->GetBrowserContext(), observer);
  }
  static void RemoveObserver(
      content::UsbDelegate* source,
      content::ServiceWorkerUsbDelegateObserver* observer) {
    source->RemoveObserver(observer->GetBrowserContext(), observer);
  }
};

}  // namespace base

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_USB_DELEGATE_OBSERVER_H_
