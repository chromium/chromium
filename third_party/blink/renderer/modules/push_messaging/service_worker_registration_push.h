// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_SERVICE_WORKER_REGISTRATION_PUSH_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_SERVICE_WORKER_REGISTRATION_PUSH_H_

#include "base/macros.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class PushManager;
class ServiceWorkerRegistration;

class ServiceWorkerRegistrationPush final
    : public GarbageCollected<ServiceWorkerRegistrationPush>,
      public Supplement<ServiceWorkerRegistration> {
  USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistrationPush);

 public:
  static const char kSupplementName[];

  explicit ServiceWorkerRegistrationPush(
      ServiceWorkerRegistration* registration);
  virtual ~ServiceWorkerRegistrationPush();
  static ServiceWorkerRegistrationPush& From(
      ServiceWorkerRegistration& registration);

  static PushManager* pushManager(ServiceWorkerRegistration& registration);
  PushManager* pushManager();

  void Trace(blink::Visitor* visitor) override;

 private:
  Member<ServiceWorkerRegistration> registration_;
  Member<PushManager> push_manager_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistrationPush);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_SERVICE_WORKER_REGISTRATION_PUSH_H_
