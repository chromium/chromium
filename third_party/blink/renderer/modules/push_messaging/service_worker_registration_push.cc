// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/service_worker_registration_push.h"

#include "third_party/blink/renderer/modules/push_messaging/push_manager.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

ServiceWorkerRegistrationPush::ServiceWorkerRegistrationPush(
    ServiceWorkerRegistration* registration)
    : Supplement(*registration) {}

ServiceWorkerRegistrationPush::~ServiceWorkerRegistrationPush() = default;

const char ServiceWorkerRegistrationPush::kSupplementName[] =
    "ServiceWorkerRegistrationPush";

ServiceWorkerRegistrationPush& ServiceWorkerRegistrationPush::From(
    ServiceWorkerRegistration& registration) {
  ServiceWorkerRegistrationPush* supplement =
      Supplement<ServiceWorkerRegistration>::From<
          ServiceWorkerRegistrationPush>(registration);
  if (!supplement) {
    supplement =
        MakeGarbageCollected<ServiceWorkerRegistrationPush>(&registration);
    ProvideTo(registration, supplement);
  }
  return *supplement;
}

PushManager* ServiceWorkerRegistrationPush::pushManager(
    ServiceWorkerRegistration& registration) {
  return ServiceWorkerRegistrationPush::From(registration).pushManager();
}

PushManager* ServiceWorkerRegistrationPush::pushManager() {
  if (!push_manager_)
    push_manager_ = MakeGarbageCollected<PushManager>(GetSupplementable());
  return push_manager_.Get();
}

void ServiceWorkerRegistrationPush::Trace(Visitor* visitor) const {
  visitor->Trace(push_manager_);
  Supplement<ServiceWorkerRegistration>::Trace(visitor);
}

}  // namespace blink
