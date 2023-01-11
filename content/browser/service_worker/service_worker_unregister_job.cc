// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_unregister_job.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

typedef ServiceWorkerRegisterJobBase::RegistrationJobType RegistrationJobType;

ServiceWorkerUnregisterJob::ServiceWorkerUnregisterJob(
    ServiceWorkerContextCore* context,
    const GURL& scope,
    const blink::StorageKey& key,
    bool is_immediate)
    : context_(context), scope_(scope), key_(key), is_immediate_(is_immediate) {
  DCHECK(context_);
}

ServiceWorkerUnregisterJob::~ServiceWorkerUnregisterJob() = default;

void ServiceWorkerUnregisterJob::AddCallback(UnregistrationCallback callback) {
  callbacks_.emplace_back(std::move(callback));
}

void ServiceWorkerUnregisterJob::Start() {
  context_->registry()->FindRegistrationForScope(
      scope_, key_,
      base::BindOnce(&ServiceWorkerUnregisterJob::OnRegistrationFound,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerUnregisterJob::Abort() {
  CompleteInternal(blink::mojom::kInvalidServiceWorkerRegistrationId,
                   blink::ServiceWorkerStatusCode::kErrorAbort);
}

bool ServiceWorkerUnregisterJob::Equals(
    ServiceWorkerRegisterJobBase* job) const {
  if (job->GetType() != GetType())
    return false;
  return static_cast<ServiceWorkerUnregisterJob*>(job)->scope_ == scope_;
}

RegistrationJobType ServiceWorkerUnregisterJob::GetType() const {
  return UNREGISTRATION_JOB;
}

void ServiceWorkerUnregisterJob::OnRegistrationFound(
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  if (status == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    DCHECK(!registration.get());
    Complete(blink::mojom::kInvalidServiceWorkerRegistrationId,
             blink::ServiceWorkerStatusCode::kErrorNotFound);
    return;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    Complete(blink::mojom::kInvalidServiceWorkerRegistrationId, status);
    return;
  }

  DCHECK(!registration->is_uninstalling());

  ResolvePromise(registration->id(), blink::ServiceWorkerStatusCode::kOk);

  if (is_immediate_)
    registration->DeleteAndClearImmediately();
  else
    registration->DeleteAndClearWhenReady();

  Complete(registration->id(), blink::ServiceWorkerStatusCode::kOk);
}

void ServiceWorkerUnregisterJob::Complete(
    int64_t registration_id,
    blink::ServiceWorkerStatusCode status) {
  CompleteInternal(registration_id, status);
  context_->job_coordinator()->FinishJob(scope_, key_, this);
}

void ServiceWorkerUnregisterJob::CompleteInternal(
    int64_t registration_id,
    blink::ServiceWorkerStatusCode status) {
  if (!is_promise_resolved_)
    ResolvePromise(registration_id, status);
}

void ServiceWorkerUnregisterJob::ResolvePromise(
    int64_t registration_id,
    blink::ServiceWorkerStatusCode status) {
  DCHECK(!is_promise_resolved_);
  is_promise_resolved_ = true;
  for (UnregistrationCallback& callback : callbacks_)
    std::move(callback).Run(registration_id, status);
  callbacks_.clear();
}

}  // namespace content
