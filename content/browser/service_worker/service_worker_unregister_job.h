// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UNREGISTER_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UNREGISTER_JOB_H_

#include <stdint.h>

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_register_job_base.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerRegistration;

// Handles the unregistration of a Service Worker.
//
// The unregistration process is primarily cleanup, removing everything that was
// created during the registration process, including the
// ServiceWorkerRegistration itself.
class ServiceWorkerUnregisterJob : public ServiceWorkerRegisterJobBase {
 public:
  typedef base::OnceCallback<void(int64_t registration_id,
                                  blink::ServiceWorkerStatusCode status)>
      UnregistrationCallback;

  // If |is_immediate| is true, unregister clears the active worker from the
  // registration without waiting for the controlled clients to unload.
  ServiceWorkerUnregisterJob(ServiceWorkerContextCore* context,
                             const GURL& scope,
                             const blink::StorageKey& key,
                             bool is_immediate);

  ServiceWorkerUnregisterJob(const ServiceWorkerUnregisterJob&) = delete;
  ServiceWorkerUnregisterJob& operator=(const ServiceWorkerUnregisterJob&) =
      delete;

  ~ServiceWorkerUnregisterJob() override;

  // Registers a callback to be called when the job completes (whether
  // successfully or not). Multiple callbacks may be registered.
  void AddCallback(UnregistrationCallback callback);

  // ServiceWorkerRegisterJobBase implementation:
  void Start() override;
  void Abort() override;
  bool Equals(ServiceWorkerRegisterJobBase* job) const override;
  RegistrationJobType GetType() const override;

 private:
  void OnRegistrationFound(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void Complete(int64_t registration_id, blink::ServiceWorkerStatusCode status);
  void CompleteInternal(int64_t registration_id,
                        blink::ServiceWorkerStatusCode status);
  void ResolvePromise(int64_t registration_id,
                      blink::ServiceWorkerStatusCode status);

  // The ServiceWorkerContextCore object must outlive this.
  const raw_ptr<ServiceWorkerContextCore> context_;
  const GURL scope_;
  const blink::StorageKey key_;
  const bool is_immediate_;
  std::vector<UnregistrationCallback> callbacks_;
  bool is_promise_resolved_ = false;
  base::WeakPtrFactory<ServiceWorkerUnregisterJob> weak_factory_{this};
};
}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UNREGISTER_JOB_H_
