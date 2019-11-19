// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_

#include <map>
#include <memory>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_unregister_job.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerRegistration;

// This class manages all in-flight registration or unregistration jobs.
class CONTENT_EXPORT ServiceWorkerJobCoordinator {
 public:
  explicit ServiceWorkerJobCoordinator(ServiceWorkerContextCore* context);
  ~ServiceWorkerJobCoordinator();

  void Register(const GURL& script_url,
                const blink::mojom::ServiceWorkerRegistrationOptions& options,
                blink::mojom::FetchClientSettingsObjectPtr
                    outside_fetch_client_settings_object,
                ServiceWorkerRegisterJob::RegistrationCallback callback);

  void Unregister(const GURL& scope,
                  ServiceWorkerUnregisterJob::UnregistrationCallback callback);

  void Update(ServiceWorkerRegistration* registration, bool force_bypass_cache);

  void Update(ServiceWorkerRegistration* registration,
              bool force_bypass_cache,
              bool skip_script_comparison,
              blink::mojom::FetchClientSettingsObjectPtr
                  outside_fetch_client_settings_object,
              ServiceWorkerRegisterJob::RegistrationCallback callback);

  // Calls ServiceWorkerRegisterJobBase::Abort() on the specified jobs (all jobs
  // for a given scope, or all jobs entirely) and removes them.
  void Abort(const GURL& scope);
  void AbortAll();

  // Marks that the ServiceWorkerContextCore is shutting down, so jobs may be
  // destroyed before finishing.
  void ClearForShutdown();

  // Removes the job. A job that was not aborted must call FinishJob when it is
  // done.
  void FinishJob(const GURL& scope, ServiceWorkerRegisterJobBase* job);

 private:
  class JobQueue {
   public:
    JobQueue();
    JobQueue(JobQueue&&);
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

    // Marks that the browser is shutting down, so jobs may be destroyed before
    // finishing.
    void ClearForShutdown();

   private:
    base::circular_deque<std::unique_ptr<ServiceWorkerRegisterJobBase>> jobs_;

    DISALLOW_COPY_AND_ASSIGN(JobQueue);
  };

  // The ServiceWorkerContextCore object must outlive this.
  ServiceWorkerContextCore* const context_;
  std::map<GURL, JobQueue> job_queues_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerJobCoordinator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_
