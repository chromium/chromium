// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_device_delegate_observer.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"

namespace content {

ServiceWorkerDeviceDelegateObserver::ServiceWorkerDeviceDelegateObserver(
    ServiceWorkerContextCore* context)
    : context_(raw_ref<ServiceWorkerContextCore>::from_ptr(context)) {
  observation_.Observe(context_->wrapper());
}

ServiceWorkerDeviceDelegateObserver::~ServiceWorkerDeviceDelegateObserver() =
    default;

void ServiceWorkerDeviceDelegateObserver::OnRegistrationDeleted(
    int64_t registration_id,
    const GURL& scope,
    const blink::StorageKey& key) {
  if (registration_id_map_.erase(registration_id) > 0) {
    RegistrationRemoved(registration_id);
  }
}

void ServiceWorkerDeviceDelegateObserver::Register(int64_t registration_id) {
  auto registration = context_->GetLiveRegistration(registration_id);
  // We should have a live registration to reach this point.
  CHECK(registration);
  if (registration_id_map_.contains(registration_id)) {
    return;
  }

  // TODO(crbug.com/1446487): Set to true only if it has the event listener.
  registration_id_map_[registration_id] = {registration->key(), true};
  RegistrationAdded(registration_id);
}

BrowserContext* ServiceWorkerDeviceDelegateObserver::GetBrowserContext() {
  return context_->wrapper()->browser_context();
}

base::WeakPtr<ServiceWorkerDeviceDelegateObserver>
ServiceWorkerDeviceDelegateObserver::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace content
