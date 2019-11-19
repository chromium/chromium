// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_unregister_job.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

typedef ServiceWorkerRegisterJobBase::RegistrationJobType RegistrationJobType;

ServiceWorkerUnregisterJob::ServiceWorkerUnregisterJob(
    ServiceWorkerContextCore* context,
    const GURL& scope)
    : context_(context), scope_(scope), is_promise_resolved_(false) {
  DCHECK(context_);
}

ServiceWorkerUnregisterJob::~ServiceWorkerUnregisterJob() {}

void ServiceWorkerUnregisterJob::AddCallback(UnregistrationCallback callback) {
  callbacks_.emplace_back(std::move(callback));
}

void ServiceWorkerUnregisterJob::Start() {
  context_->storage()->FindRegistrationForScope(
      scope_, base::BindOnce(&ServiceWorkerUnregisterJob::OnRegistrationFound,
                             weak_factory_.GetWeakPtr()));
}

void ServiceWorkerUnregisterJob::Abort() {
  CompleteInternal(blink::mojom::kInvalidServiceWorkerRegistrationId,
                   blink::ServiceWorkerStatusCode::kErrorAbort);
}

void ServiceWorkerUnregisterJob::WillShutDown() {}

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

  if (status != blink::ServiceWorkerStatusCode::kOk ||
      registration->is_uninstalling()) {
    Complete(blink::mojom::kInvalidServiceWorkerRegistrationId, status);
    return;
  }

  // TODO: "7. If registration.updatePromise is not null..."

  // "8. Resolve promise."
  ResolvePromise(registration->id(), blink::ServiceWorkerStatusCode::kOk);

  registration->ClearWhenReady();

  Complete(registration->id(), blink::ServiceWorkerStatusCode::kOk);
}

void ServiceWorkerUnregisterJob::Complete(
    int64_t registration_id,
    blink::ServiceWorkerStatusCode status) {
  CompleteInternal(registration_id, status);
  context_->job_coordinator()->FinishJob(scope_, this);
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
}

}  // namespace content
