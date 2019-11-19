// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_INDEX_SERVICE_WORKER_REGISTRATION_CONTENT_INDEX_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_INDEX_SERVICE_WORKER_REGISTRATION_CONTENT_INDEX_H_

#include "base/macros.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ContentIndex;

class ServiceWorkerRegistrationContentIndex final
    : public GarbageCollected<ServiceWorkerRegistrationContentIndex>,
      public Supplement<ServiceWorkerRegistration> {
  USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistrationContentIndex);

 public:
  static const char kSupplementName[];

  explicit ServiceWorkerRegistrationContentIndex(
      ServiceWorkerRegistration* registration);

  static ServiceWorkerRegistrationContentIndex& From(
      ServiceWorkerRegistration& registration);

  static ContentIndex* index(ServiceWorkerRegistration& registration);
  ContentIndex* index();

  void Trace(blink::Visitor* visitor) override;

 private:
  Member<ServiceWorkerRegistration> registration_;
  Member<ContentIndex> content_index_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistrationContentIndex);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_INDEX_SERVICE_WORKER_REGISTRATION_CONTENT_INDEX_H_
