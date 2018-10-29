// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_CONTAINER_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_CONTAINER_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class WebServiceWorkerProvider;

// This mainly exists to provide access to WebServiceWorkerProvider.
// Owned by Document.
class MODULES_EXPORT ServiceWorkerContainerClient final
    : public GarbageCollectedFinalized<ServiceWorkerContainerClient>,
      public Supplement<Document>,
      public NameClient {
  USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerContainerClient);

 public:
  static const char kSupplementName[];

  ServiceWorkerContainerClient(Document&,
                               std::unique_ptr<WebServiceWorkerProvider>);
  virtual ~ServiceWorkerContainerClient();

  // Returns the ServiceWorkerRegistration object described by the object info
  // in current execution context. Creates a new object if needed, or else
  // returns the existing one.
  ServiceWorkerRegistration* GetOrCreateServiceWorkerRegistration(
      WebServiceWorkerRegistrationObjectInfo);

  // Returns the ServiceWorker object described by the object info in current
  // execution context. Creates a new object if needed, or else returns the
  // existing one.
  ServiceWorker* GetOrCreateServiceWorker(WebServiceWorkerObjectInfo);

  WebServiceWorkerProvider* Provider() { return provider_.get(); }

  static ServiceWorkerContainerClient* From(Document*);

  void Trace(blink::Visitor* visitor) override;

  const char* NameInHeapSnapshot() const override {
    return "ServiceWorkerContainerClient";
  }

 private:
  std::unique_ptr<WebServiceWorkerProvider> provider_;
  // Map from service worker registration id to JavaScript
  // ServiceWorkerRegistration object in current execution context.
  HeapHashMap<int64_t,
              WeakMember<ServiceWorkerRegistration>,
              WTF::IntHash<int64_t>,
              WTF::UnsignedWithZeroKeyHashTraits<int64_t>>
      service_worker_registration_objects_;
  // Map from service worker version id to JavaScript ServiceWorker object in
  // current execution context.
  HeapHashMap<int64_t,
              WeakMember<ServiceWorker>,
              WTF::IntHash<int64_t>,
              WTF::UnsignedWithZeroKeyHashTraits<int64_t>>
      service_worker_objects_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContainerClient);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_CONTAINER_CLIENT_H_
