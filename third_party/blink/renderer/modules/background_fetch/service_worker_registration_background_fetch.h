// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_FETCH_SERVICE_WORKER_REGISTRATION_BACKGROUND_FETCH_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_FETCH_SERVICE_WORKER_REGISTRATION_BACKGROUND_FETCH_H_

#include "base/macros.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class BackgroundFetchManager;

class ServiceWorkerRegistrationBackgroundFetch final
    : public GarbageCollected<ServiceWorkerRegistrationBackgroundFetch>,
      public Supplement<ServiceWorkerRegistration> {
  USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistrationBackgroundFetch);

 public:
  static const char kSupplementName[];

  explicit ServiceWorkerRegistrationBackgroundFetch(
      ServiceWorkerRegistration* registration);
  virtual ~ServiceWorkerRegistrationBackgroundFetch();

  static ServiceWorkerRegistrationBackgroundFetch& From(
      ServiceWorkerRegistration& registration);

  static BackgroundFetchManager* backgroundFetch(
      ServiceWorkerRegistration& registration);
  BackgroundFetchManager* backgroundFetch();

  void Trace(blink::Visitor* visitor) override;

 private:
  Member<ServiceWorkerRegistration> registration_;
  Member<BackgroundFetchManager> background_fetch_manager_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistrationBackgroundFetch);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_FETCH_SERVICE_WORKER_REGISTRATION_BACKGROUND_FETCH_H_
