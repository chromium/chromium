// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_

#include <map>
#include <memory>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_unregister_job.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_ancestor_frame_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom-forward.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerRegistration;

// This class manages all in-flight registration or unregistration jobs.
class CONTENT_EXPORT ServiceWorkerJobCoordinator {
 public:
  explicit ServiceWorkerJobCoordinator(ServiceWorkerContextCore* context);

  ServiceWorkerJobCoordinator(const ServiceWorkerJobCoordinator&) = delete;
  ServiceWorkerJobCoordinator& operator=(const ServiceWorkerJobCoordinator&) =
      delete;

  ~ServiceWorkerJobCoordinator();

  void Register(const GURL& script_url,
                const blink::mojom::ServiceWorkerRegistrationOptions& options,
                const blink::StorageKey& key,
                blink::mojom::FetchClientSettingsObjectPtr
                    outside_fetch_client_settings_object,
                const GlobalRenderFrameHostId& requesting_frame_id,
                blink::mojom::AncestorFrameType ancestor_frame_type,
                ServiceWorkerRegisterJob::RegistrationCallback callback,
                const PolicyContainerPolicies& policy_container_policies);

  // If |is_immediate| is true, unregister clears the active worker from the
  // registration without waiting for the controlled clients to unload.
  void Unregister(const GURL& scope,
                  const blink::StorageKey& key,
                  bool is_immediate,
                  ServiceWorkerUnregisterJob::UnregistrationCallback callback);

  void Update(ServiceWorkerRegistration* registration,
              bool force_bypass_cache,
              bool skip_script_comparison,
              blink::mojom::FetchClientSettingsObjectPtr
                  outside_fetch_client_settings_object,
              ServiceWorkerRegisterJob::RegistrationCallback callback);

  // Calls ServiceWorkerRegisterJobBase::Abort() on the specified jobs (all jobs
  // for a given scope, or all jobs entirely) and removes them.
  void Abort(const GURL& scope, const blink::StorageKey& key);
  void AbortAll();

  // Removes the job. A job that was not aborted must call FinishJob when it is
  // done.
  void FinishJob(const GURL& scope,
                 const blink::StorageKey& key,
                 ServiceWorkerRegisterJobBase* job);

 private:
  // A given service worker's registration is uniqely identified by the scope
  // url and the storage key.
  using UniqueRegistrationKey = std::pair<GURL, blink::StorageKey>;

  class JobQueue {
   public:
    JobQueue();
    JobQueue(JobQueue&&);

    JobQueue(const JobQueue&) = delete;
    JobQueue& operator=(const JobQueue&) = delete;

    ~JobQueue();

    // Adds a job to the queue. If an identical job is already at the end of the
    // queue, no new job is added. Returns the job in the queue, regardless of
    // whether it was newly added.
    ServiceWorkerRegisterJobBase* Push(
        std::unique_ptr<ServiceWorkerRegisterJobBase> job);

    // Starts the first job in the queue.
    void StartOneJob();

    // Removes a job from the queue.
    void Pop(ServiceWorkerRegisterJobBase* job);

    bool empty() { return jobs_.empty(); }

    // Aborts all jobs in the queue and removes them.
    void AbortAll();

   private:
    base::circular_deque<std::unique_ptr<ServiceWorkerRegisterJobBase>> jobs_;
  };

  // The ServiceWorkerContextCore object must outlive this.
  const raw_ptr<ServiceWorkerContextCore> context_;
  std::map<UniqueRegistrationKey, JobQueue> job_queues_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_
