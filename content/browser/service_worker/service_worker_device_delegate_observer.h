// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DEVICE_DELEGATE_OBSERVER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DEVICE_DELEGATE_OBSERVER_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

class ServiceWorkerContextCore;

class CONTENT_EXPORT ServiceWorkerDeviceDelegateObserver
    : public ServiceWorkerContextCoreObserver {
 public:
  struct RegistrationInfo {
    blink::StorageKey key;
    bool has_event_listener;

    bool operator==(const RegistrationInfo& other) const {
      return key == other.key && has_event_listener == other.has_event_listener;
    }
  };
  using RegistrationIdMap = base::flat_map<int64_t, RegistrationInfo>;

  explicit ServiceWorkerDeviceDelegateObserver(
      ServiceWorkerContextCore* context);
  ServiceWorkerDeviceDelegateObserver(
      const ServiceWorkerDeviceDelegateObserver&) = delete;
  ServiceWorkerDeviceDelegateObserver& operator=(
      const ServiceWorkerDeviceDelegateObserver&) = delete;
  ~ServiceWorkerDeviceDelegateObserver() override;

  // ServiceWorkerContextCoreObserver:
  void OnRegistrationDeleted(int64_t registration_id,
                             const GURL& scope,
                             const blink::StorageKey& key) override;

  // Register the ServiceWorkerRegistration with `registration_id` to respond to
  // device delegate observer.
  void Register(int64_t registration_id);

  BrowserContext* GetBrowserContext();

  base::WeakPtr<ServiceWorkerDeviceDelegateObserver> GetWeakPtr();

  const RegistrationIdMap& registration_id_map() {
    return registration_id_map_;
  }

 private:
  // It is called when `registration_id` is added to `registration_id_map_`.
  virtual void RegistrationAdded(int64_t registration_id) = 0;

  // It is called when `registration_id` is removed to `registration_id_map_`.
  virtual void RegistrationRemoved(int64_t registration_id) = 0;

  // The map stores ids of service worker registrations that need to respond to
  // device delegate observer.
  RegistrationIdMap registration_id_map_;

  // ServiceWorkerDeviceDelegateObserver is owned by ServiceWorkerContextCore.
  const raw_ref<ServiceWorkerContextCore> context_;

  base::ScopedObservation<ServiceWorkerContextWrapper,
                          ServiceWorkerContextCoreObserver>
      observation_{this};

  base::WeakPtrFactory<ServiceWorkerDeviceDelegateObserver> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DEVICE_DELEGATE_OBSERVER_H_
