// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_device_delegate_observer.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace content {

namespace {

void DidFindRegistration(
    ServiceWorkerDeviceDelegateObserver::ServiceWorkerStartedCallback callback,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(nullptr, service_worker_status);
    return;
  }

  ServiceWorkerVersion* active_version =
      service_worker_registration->active_version();
  CHECK(active_version);
  active_version->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::EXTERNAL_REQUEST,
      base::BindOnce(std::move(callback),
                     base::WrapRefCounted(active_version)));
}

}  // namespace

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

void ServiceWorkerDeviceDelegateObserver::OnStopped(int64_t version_id) {
  pending_callbacks_.erase(version_id);
}

void ServiceWorkerDeviceDelegateObserver::Register(int64_t registration_id) {
  auto registration = context_->GetLiveRegistration(registration_id);
  // We should have a live registration to reach this point.
  CHECK(registration);
  if (registration_id_map_.contains(registration_id)) {
    return;
  }

  // By default, the
  // `registration_id_map_[registration_id].has_hid_event_handlers` is set to
  // false. It relies on the activated ServiceWorkerVersion to use
  // `UpdateHasEventHandlers` for updating it.
  registration_id_map_[registration_id] = {registration->key(), false};
  RegistrationAdded(registration_id);
}

void ServiceWorkerDeviceDelegateObserver::DispatchEventToWorker(
    int64_t registration_id,
    ServiceWorkerStartedCallback callback) {
  auto it = registration_id_map_.find(registration_id);
  CHECK(it != registration_id_map_.end());
  context_->wrapper()->FindReadyRegistrationForId(
      registration_id, it->second.key,
      base::BindOnce(&DidFindRegistration, std::move(callback)));
}

void ServiceWorkerDeviceDelegateObserver::UpdateHasEventHandlers(
    int64_t registration_id,
    bool has_event_handlers) {
  auto it = registration_id_map_.find(registration_id);
  // This can happen when an installed service worker registration with device
  // event handlers is loaded after browser restart. In this case, it only
  // registers the service worker that has device event handlers.
  if (it == registration_id_map_.end()) {
    if (!has_event_handlers) {
      return;
    }
    Register(registration_id);
    it = registration_id_map_.find(registration_id);
  }
  it->second.has_event_handlers = has_event_handlers;
}

void ServiceWorkerDeviceDelegateObserver::ProcessPendingCallbacks(
    ServiceWorkerVersion* version) {
  CHECK(version);
  auto it = pending_callbacks_.find(version->version_id());
  if (it == pending_callbacks_.end()) {
    return;
  }
  auto callbacks = std::move(it->second);
  pending_callbacks_.erase(it);
  for (auto& cb : callbacks) {
    std::move(cb).Run();
  }
}

void ServiceWorkerDeviceDelegateObserver::AddPendingCallback(
    ServiceWorkerVersion* version,
    base::OnceClosure callback) {
  // This should only be used when the worker is running since it is meant to be
  // used in the case of the worker is running and caller is waiting other
  // states to be ready for executing `callback`.
  CHECK(version);
  CHECK_EQ(version->running_status(), blink::EmbeddedWorkerStatus::kRunning);
  pending_callbacks_[version->version_id()].push_back(std::move(callback));
}

BrowserContext* ServiceWorkerDeviceDelegateObserver::GetBrowserContext() {
  return context_->wrapper()->browser_context();
}


}  // namespace content
