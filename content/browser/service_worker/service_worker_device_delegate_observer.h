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
  using ServiceWorkerStartedCallback =
      base::OnceCallback<void(scoped_refptr<ServiceWorkerVersion>,
                              blink::ServiceWorkerStatusCode)>;
  struct RegistrationInfo {
    blink::StorageKey key;
    bool has_event_handlers;

    bool operator==(const RegistrationInfo& other) const {
      return key == other.key && has_event_handlers == other.has_event_handlers;
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
  void OnStopped(int64_t version_id) override;

  // Register the ServiceWorkerRegistration with `registration_id` to respond to
  // device delegate observer.
  void Register(int64_t registration_id);

  // Start the worker with `registration_id` and run `callback` on the worker.
  void DispatchEventToWorker(int64_t registration_id,
                             ServiceWorkerStartedCallback callback);

  // Set `registration_id_map_[registration_id].has_event_handlers` to
  // `has_event_handlers`. If `registration_id` isn't in
  // `registration_id_map_`. It will call `Register(registration_id)` to
  // register it.
  void UpdateHasEventHandlers(int64_t registration_id, bool has_event_handlers);

  // Process pending callbacked stored in
  // `pending_callbacks_[version->version_id()]`.
  void ProcessPendingCallbacks(ServiceWorkerVersion* version);

  // Add `callback` to `pending_callbacks_[version->version_id()]`.
  void AddPendingCallback(ServiceWorkerVersion* version,
                          base::OnceClosure callback);

  BrowserContext* GetBrowserContext();

  const RegistrationIdMap& registration_id_map() {
    return registration_id_map_;
  }

  const base::flat_map<int64_t, std::vector<base::OnceClosure>>&
  GetPendingCallbacksForTesting() {
    return pending_callbacks_;
  }

 private:
  // It is called when `registration_id` is added to `registration_id_map_`.
  virtual void RegistrationAdded(int64_t registration_id) = 0;

  // It is called when `registration_id` is removed to `registration_id_map_`.
  virtual void RegistrationRemoved(int64_t registration_id) = 0;

  // The map stores ids of service worker registrations that need to respond to
  // device delegate observer.
  RegistrationIdMap registration_id_map_;

  // `pending_callbacks_` is a buffer for storing callbacks when some states are
  // not yet reached even when the worker is running.
  // For example, when the script is evaluated and the service worker is
  // running, the inter-process request that registers to HidService for
  // renderer client might not be done. Callbacks `pending_callbacks_` here can
  // be used as a buffer to hold these callbacks until the client registration
  // completes.
  // `pending_callbacks_[version_id]` will be cleared when the worker with
  // `version_id` stopped.
  base::flat_map<int64_t, std::vector<base::OnceClosure>> pending_callbacks_;

  // ServiceWorkerDeviceDelegateObserver is owned by ServiceWorkerContextCore.
  const raw_ref<ServiceWorkerContextCore> context_;

  base::ScopedObservation<ServiceWorkerContextWrapper,
                          ServiceWorkerContextCoreObserver>
      observation_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DEVICE_DELEGATE_OBSERVER_H_
