// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HID_DELEGATE_OBSERVER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HID_DELEGATE_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation_traits.h"
#include "content/browser/hid/hid_service.h"
#include "content/browser/service_worker/service_worker_device_delegate_observer.h"
#include "content/public/browser/hid_delegate.h"

namespace content {

class ServiceWorkerContextCore;

// ServiceWorkerHidDelegateObserver acts as a broker between the
// content::HidService and content::HidDelegate when the HidService is created
// for a service worker.
// Each ServiceWorkerContextCore has one ServiceWorkerHidDelegateObserver,
// which is used to track all of the ServiceWorkerRegistration objects whose
// script uses the WebHID API.
// Furthermore, when a device event happens, the
// ServiceWorkerHidDelegateObserver's HidDelegate::Observer methods will be
// invoked and deliver the device events to the service worker that has a device
// event handler registered.
// For more information, please see go/usb-hid-extension-access.
class CONTENT_EXPORT ServiceWorkerHidDelegateObserver
    : public ServiceWorkerDeviceDelegateObserver,
      public HidDelegate::Observer {
 public:
  explicit ServiceWorkerHidDelegateObserver(ServiceWorkerContextCore* context);
  ServiceWorkerHidDelegateObserver(const ServiceWorkerHidDelegateObserver&) =
      delete;
  ServiceWorkerHidDelegateObserver& operator=(
      const ServiceWorkerHidDelegateObserver&) = delete;
  ~ServiceWorkerHidDelegateObserver() override;

  // HidDelegate::Observer:
  void OnDeviceAdded(const device::mojom::HidDeviceInfo& device_info) override;
  void OnDeviceRemoved(
      const device::mojom::HidDeviceInfo& device_info) override;
  void OnDeviceChanged(
      const device::mojom::HidDeviceInfo& device_info) override;
  void OnHidManagerConnectionError() override;
  void OnPermissionRevoked(const url::Origin& origin) override;

  // Register the `hid_service` to be the latest HidService for
  // `registraiton_id`.
  void RegisterHidService(int64_t registration_id,
                          base::WeakPtr<HidService> hid_service);

  HidService* GetHidServiceForTesting(int64_t registration_id) {
    return GetHidService(registration_id);
  }

 private:
  using HidServiceCallback = base::OnceCallback<void(HidService*)>;
  using HidServiceDeviceEventCallback =
      base::RepeatingCallback<void(const device::mojom::HidDeviceInfo&,
                                   HidService*)>;

  // ServiceWorkerDeviceDelegateObserver:
  void RegistrationAdded(int64_t registration_id) override;
  void RegistrationRemoved(int64_t registration_id) override;

  // Dispatch the device event to all registered service workers.
  void DispatchHidDeviceEventToWorkers(
      const device::mojom::HidDeviceInfo& device_info,
      HidServiceDeviceEventCallback callback);

  // Run `callback` with `device_info` after the worker of `version` is started.
  void WorkerStarted(device::mojom::HidDeviceInfoPtr device_info,
                     HidServiceDeviceEventCallback callback,
                     scoped_refptr<ServiceWorkerVersion> version,
                     blink::ServiceWorkerStatusCode service_worker_status);

  // Get HidService for the `registration_id`. It can be null if no live
  // HidService for the `registration_id`.
  HidService* GetHidService(int64_t registration_id);

  // The map for registration id to the latest registered HidService.
  base::flat_map<int64_t, base::WeakPtr<HidService>> hid_services_;

  base::ScopedObservation<HidDelegate, ServiceWorkerHidDelegateObserver>
      hid_delegate_observation{this};

  base::WeakPtrFactory<ServiceWorkerHidDelegateObserver> weak_ptr_factory_{
      this};
};

}  // namespace content

namespace base {

template <>
struct ScopedObservationTraits<content::HidDelegate,
                               content::ServiceWorkerHidDelegateObserver> {
  static void AddObserver(content::HidDelegate* source,
                          content::ServiceWorkerHidDelegateObserver* observer) {
    source->AddObserver(observer->GetBrowserContext(), observer);
  }
  static void RemoveObserver(
      content::HidDelegate* source,
      content::ServiceWorkerHidDelegateObserver* observer) {
    source->RemoveObserver(observer->GetBrowserContext(), observer);
  }
};

}  // namespace base

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HID_DELEGATE_OBSERVER_H_
