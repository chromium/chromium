// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_index/service_worker_registration_content_index.h"

#include "third_party/blink/renderer/modules/content_index/content_index.h"

namespace blink {

ServiceWorkerRegistrationContentIndex::ServiceWorkerRegistrationContentIndex(
    ServiceWorkerRegistration* registration)
    : Supplement(*registration) {}

const char ServiceWorkerRegistrationContentIndex::kSupplementName[] =
    "ServiceWorkerRegistrationContentIndex";

ServiceWorkerRegistrationContentIndex&
ServiceWorkerRegistrationContentIndex::From(
    ServiceWorkerRegistration& registration) {
  ServiceWorkerRegistrationContentIndex* supplement =
      Supplement<ServiceWorkerRegistration>::From<
          ServiceWorkerRegistrationContentIndex>(registration);

  if (!supplement) {
    supplement = MakeGarbageCollected<ServiceWorkerRegistrationContentIndex>(
        &registration);
    ProvideTo(registration, supplement);
  }

  return *supplement;
}

ContentIndex* ServiceWorkerRegistrationContentIndex::index(
    ServiceWorkerRegistration& registration) {
  return ServiceWorkerRegistrationContentIndex::From(registration).index();
}

ContentIndex* ServiceWorkerRegistrationContentIndex::index() {
  if (!content_index_) {
    ExecutionContext* execution_context =
        GetSupplementable()->GetExecutionContext();
    // TODO(falken): Consider defining a task source in the spec for this event.
    content_index_ = MakeGarbageCollected<ContentIndex>(
        GetSupplementable(),
        execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  }

  return content_index_.Get();
}

void ServiceWorkerRegistrationContentIndex::Trace(Visitor* visitor) const {
  visitor->Trace(content_index_);
  Supplement<ServiceWorkerRegistration>::Trace(visitor);
}

}  // namespace blink
