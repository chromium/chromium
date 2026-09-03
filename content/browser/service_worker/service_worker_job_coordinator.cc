// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_job_coordinator.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "content/browser/service_worker/service_worker_register_job_base.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"

namespace content {

ServiceWorkerJobCoordinator::JobQueue::JobQueue() = default;

ServiceWorkerJobCoordinator::JobQueue::JobQueue(JobQueue&&) = default;

ServiceWorkerJobCoordinator::JobQueue::~JobQueue() {
  DCHECK(jobs_.empty()) << "Destroying JobQueue with " << jobs_.size()
                        << " unfinished jobs";
}

ServiceWorkerRegisterJobBase* ServiceWorkerJobCoordinator::JobQueue::Push(
    std::unique_ptr<ServiceWorkerRegisterJobBase> job) {
  if (jobs_.empty()) {
    jobs_.push_back(std::move(job));
    StartOneJob();
  } else if (!job->Equals(jobs_.back().get())) {
    jobs_.push_back(std::move(job));
  }
  // Note we are releasing 'job' here in case neither of the two if() statements
  // above were true.

  CHECK(!jobs_.empty(), base::NotFatalUntil::M159);
  return jobs_.back().get();
}

void ServiceWorkerJobCoordinator::JobQueue::Pop(
    ServiceWorkerRegisterJobBase* job) {
  CHECK(job == jobs_.front().get(), base::NotFatalUntil::M159);
  jobs_.pop_front();
  if (!jobs_.empty()) {
    StartOneJob();
  }
}

void ServiceWorkerJobCoordinator::JobQueue::StartOneJob() {
  CHECK(!jobs_.empty(), base::NotFatalUntil::M159);
  jobs_.front()->Start();
}

void ServiceWorkerJobCoordinator::JobQueue::AbortAll() {
  while (!jobs_.empty()) {
    std::unique_ptr<ServiceWorkerRegisterJobBase> job =
        std::move(jobs_.front());
    jobs_.pop_front();
    job->Abort();
  }
}

ServiceWorkerJobCoordinator::ServiceWorkerJobCoordinator(
    ServiceWorkerContextCore* context)
    : context_(context) {
  CHECK(context_, base::NotFatalUntil::M159);
}

ServiceWorkerJobCoordinator::~ServiceWorkerJobCoordinator() {
  DCHECK(job_queues_.empty()) << "Destroying ServiceWorkerJobCoordinator with "
                              << job_queues_.size() << " job queues";
}

void ServiceWorkerJobCoordinator::Register(
    const GURL& script_url,
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    const blink::StorageKey& key,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    const GlobalRenderFrameHostId& requesting_frame_id,
    blink::mojom::AncestorFrameType ancestor_frame_type,
    ServiceWorkerRegisterJob::RegistrationCallback callback,
    const PolicyContainerPolicies& policy_container_policies) {
  auto job = std::make_unique<ServiceWorkerRegisterJob>(
      context_, script_url, options, key,
      std::move(outside_fetch_client_settings_object), requesting_frame_id,
      ancestor_frame_type, policy_container_policies.Clone());
  ServiceWorkerRegisterJob* queued_job = static_cast<ServiceWorkerRegisterJob*>(
      job_queues_[UniqueRegistrationKey(options.scope, key)].Push(
          std::move(job)));
  queued_job->AddCallback(std::move(callback));
}

void ServiceWorkerJobCoordinator::Unregister(
    const GURL& scope,
    const blink::StorageKey& key,
    bool is_immediate,
    ServiceWorkerRegistration::DeleteInitiator initiator,
    ServiceWorkerUnregisterJob::UnregistrationCallback callback) {
  std::unique_ptr<ServiceWorkerRegisterJobBase> job(
      new ServiceWorkerUnregisterJob(context_, scope, key, is_immediate,
                                     initiator));
  ServiceWorkerUnregisterJob* queued_job =
      static_cast<ServiceWorkerUnregisterJob*>(
          job_queues_[UniqueRegistrationKey(scope, key)].Push(std::move(job)));
  queued_job->AddCallback(std::move(callback));
}

void ServiceWorkerJobCoordinator::Update(
    ServiceWorkerRegistration* registration,
    bool force_bypass_cache,
    bool skip_script_comparison,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    ServiceWorkerRegisterJob::RegistrationCallback callback) {
  CHECK(registration, base::NotFatalUntil::M159);
  ServiceWorkerRegisterJob* queued_job = static_cast<ServiceWorkerRegisterJob*>(
      job_queues_[UniqueRegistrationKey(registration->scope(),
                                        registration->key())]
          .Push(base::WrapUnique<ServiceWorkerRegisterJobBase>(
              new ServiceWorkerRegisterJob(
                  context_, registration, force_bypass_cache,
                  skip_script_comparison,
                  std::move(outside_fetch_client_settings_object)))));
  if (callback) {
    queued_job->AddCallback(std::move(callback));
  }
}

void ServiceWorkerJobCoordinator::Abort(const GURL& scope,
                                        const blink::StorageKey& key) {
  UniqueRegistrationKey reg_key(scope, key);
  while (true) {
    auto pending_jobs = job_queues_.find(reg_key);
    if (pending_jobs == job_queues_.end()) {
      return;
    }
    JobQueue queue = std::move(pending_jobs->second);
    job_queues_.erase(pending_jobs);
    queue.AbortAll();
  }
}

void ServiceWorkerJobCoordinator::AbortAll() {
  while (!job_queues_.empty()) {
    auto it = job_queues_.begin();
    JobQueue queue = std::move(it->second);
    job_queues_.erase(it);
    queue.AbortAll();
  }
}

void ServiceWorkerJobCoordinator::FinishJob(const GURL& scope,
                                            const blink::StorageKey& key,
                                            ServiceWorkerRegisterJobBase* job) {
  UniqueRegistrationKey reg_key(scope, key);
  auto pending_jobs = job_queues_.find(reg_key);
  CHECK(pending_jobs != job_queues_.end()) << "Deleting non-existent job.";
  pending_jobs->second.Pop(job);
  pending_jobs = job_queues_.find(reg_key);
  if (pending_jobs != job_queues_.end() && pending_jobs->second.empty()) {
    job_queues_.erase(pending_jobs);
  }
}

}  // namespace content
