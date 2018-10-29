// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_job_coordinator.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "content/browser/service_worker/service_worker_register_job_base.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

namespace {

bool IsRegisterJob(const ServiceWorkerRegisterJobBase& job) {
  return job.GetType() == ServiceWorkerRegisterJobBase::REGISTRATION_JOB;
}

}  // namespace

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
    DoomInstallingWorkerIfNeeded();
  }
  // Note we are releasing 'job' here in case neither of the two if() statements
  // above were true.

  DCHECK(!jobs_.empty());
  return jobs_.back().get();
}

void ServiceWorkerJobCoordinator::JobQueue::Pop(
    ServiceWorkerRegisterJobBase* job) {
  DCHECK(job == jobs_.front().get());
  jobs_.pop_front();
  if (!jobs_.empty())
    StartOneJob();
}

void ServiceWorkerJobCoordinator::JobQueue::DoomInstallingWorkerIfNeeded() {
  DCHECK(!jobs_.empty());
  if (!IsRegisterJob(*jobs_.front().get()))
    return;
  ServiceWorkerRegisterJob* job =
      static_cast<ServiceWorkerRegisterJob*>(jobs_.front().get());
  auto it = jobs_.begin();
  for (++it; it != jobs_.end(); ++it) {
    if (IsRegisterJob(**it)) {
      job->DoomInstallingWorker();
      return;
    }
  }
}

void ServiceWorkerJobCoordinator::JobQueue::StartOneJob() {
  DCHECK(!jobs_.empty());
  jobs_.front()->Start();
  DoomInstallingWorkerIfNeeded();
}

void ServiceWorkerJobCoordinator::JobQueue::AbortAll() {
  for (const auto& job : jobs_)
    job->Abort();
  jobs_.clear();
}

void ServiceWorkerJobCoordinator::JobQueue::ClearForShutdown() {
  jobs_.clear();
}

ServiceWorkerJobCoordinator::ServiceWorkerJobCoordinator(
    base::WeakPtr<ServiceWorkerContextCore> context)
    : context_(context) {
}

ServiceWorkerJobCoordinator::~ServiceWorkerJobCoordinator() {
  if (!context_) {
    for (auto& job_pair : job_queues_)
      job_pair.second.ClearForShutdown();
    job_queues_.clear();
  }
  DCHECK(job_queues_.empty()) << "Destroying ServiceWorkerJobCoordinator with "
                              << job_queues_.size() << " job queues";
}

void ServiceWorkerJobCoordinator::Register(
    const GURL& script_url,
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    ServiceWorkerRegisterJob::RegistrationCallback callback) {
  std::unique_ptr<ServiceWorkerRegisterJobBase> job(
      new ServiceWorkerRegisterJob(context_, script_url, options));
  ServiceWorkerRegisterJob* queued_job = static_cast<ServiceWorkerRegisterJob*>(
      job_queues_[options.scope].Push(std::move(job)));
  queued_job->AddCallback(std::move(callback));
}

void ServiceWorkerJobCoordinator::Unregister(
    const GURL& scope,
    ServiceWorkerUnregisterJob::UnregistrationCallback callback) {
  std::unique_ptr<ServiceWorkerRegisterJobBase> job(
      new ServiceWorkerUnregisterJob(context_, scope));
  ServiceWorkerUnregisterJob* queued_job =
      static_cast<ServiceWorkerUnregisterJob*>(
          job_queues_[scope].Push(std::move(job)));
  queued_job->AddCallback(std::move(callback));
}

void ServiceWorkerJobCoordinator::Update(
    ServiceWorkerRegistration* registration,
    bool force_bypass_cache) {
  DCHECK(registration);
  job_queues_[registration->scope()].Push(
      base::WrapUnique<ServiceWorkerRegisterJobBase>(
          new ServiceWorkerRegisterJob(context_, registration,
                                       force_bypass_cache,
                                       false /* skip_script_comparison */)));
}

void ServiceWorkerJobCoordinator::Update(
    ServiceWorkerRegistration* registration,
    bool force_bypass_cache,
    bool skip_script_comparison,
    ServiceWorkerRegisterJob::RegistrationCallback callback) {
  DCHECK(registration);
  ServiceWorkerRegisterJob* queued_job = static_cast<ServiceWorkerRegisterJob*>(
      job_queues_[registration->scope()].Push(
          base::WrapUnique<ServiceWorkerRegisterJobBase>(
              new ServiceWorkerRegisterJob(context_, registration,
                                           force_bypass_cache,
                                           skip_script_comparison))));
  queued_job->AddCallback(std::move(callback));
}

void ServiceWorkerJobCoordinator::AbortAll() {
  for (auto& job_pair : job_queues_)
    job_pair.second.AbortAll();
  job_queues_.clear();
}

void ServiceWorkerJobCoordinator::FinishJob(const GURL& scope,
                                            ServiceWorkerRegisterJobBase* job) {
  auto pending_jobs = job_queues_.find(scope);
  DCHECK(pending_jobs != job_queues_.end()) << "Deleting non-existent job.";
  pending_jobs->second.Pop(job);
  if (pending_jobs->second.empty())
    job_queues_.erase(pending_jobs);
}

}  // namespace content
