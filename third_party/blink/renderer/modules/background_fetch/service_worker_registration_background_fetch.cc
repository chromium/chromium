// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/service_worker_registration_background_fetch.h"

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_manager.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

ServiceWorkerRegistrationBackgroundFetch::
    ServiceWorkerRegistrationBackgroundFetch(
        ServiceWorkerRegistration* registration)
    : service_worker_registration_(*registration) {}

ServiceWorkerRegistrationBackgroundFetch::
    ~ServiceWorkerRegistrationBackgroundFetch() = default;

ServiceWorkerRegistrationBackgroundFetch&
ServiceWorkerRegistrationBackgroundFetch::From(
    ServiceWorkerRegistration& registration) {
  ServiceWorkerRegistrationBackgroundFetch* supplement =
      registration.GetServiceWorkerRegistrationBackgroundFetch();

  if (!supplement) {
    supplement = MakeGarbageCollected<ServiceWorkerRegistrationBackgroundFetch>(
        &registration);
    registration.SetServiceWorkerRegistrationBackgroundFetch(supplement);
  }

  return *supplement;
}

BackgroundFetchManager*
ServiceWorkerRegistrationBackgroundFetch::backgroundFetch(
    ServiceWorkerRegistration& registration) {
  return ServiceWorkerRegistrationBackgroundFetch::From(registration)
      .backgroundFetch();
}

BackgroundFetchManager*
ServiceWorkerRegistrationBackgroundFetch::backgroundFetch() {
  if (!background_fetch_manager_) {
    background_fetch_manager_ = MakeGarbageCollected<BackgroundFetchManager>(
        service_worker_registration_);
  }

  return background_fetch_manager_.Get();
}

void ServiceWorkerRegistrationBackgroundFetch::Trace(Visitor* visitor) const {
  visitor->Trace(background_fetch_manager_);
  visitor->Trace(service_worker_registration_);
}

}  // namespace blink
