// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NOTIFICATIONS_SERVICE_WORKER_REGISTRATION_NOTIFICATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NOTIFICATIONS_SERVICE_WORKER_REGISTRATION_NOTIFICATIONS_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExecutionContext;
class ExceptionState;
class GetNotificationOptions;
class NotificationOptions;
class NotificationResourcesLoader;
class ScriptPromiseResolver;
class ScriptState;
class SecurityOrigin;
class ServiceWorkerRegistration;

class ServiceWorkerRegistrationNotifications final
    : public GarbageCollected<ServiceWorkerRegistrationNotifications>,
      public Supplement<ServiceWorkerRegistration>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistrationNotifications);

 public:
  static const char kSupplementName[];

  static ScriptPromise showNotification(ScriptState* script_state,
                                        ServiceWorkerRegistration& registration,
                                        const String& title,
                                        const NotificationOptions* options,
                                        ExceptionState& exception_state);
  static ScriptPromise getNotifications(ScriptState* script_state,
                                        ServiceWorkerRegistration& registration,
                                        const GetNotificationOptions* options);

  ServiceWorkerRegistrationNotifications(ExecutionContext*,
                                         ServiceWorkerRegistration*);

  // ContextLifecycleObserver interface.
  void ContextDestroyed(ExecutionContext* context) override;

  void Trace(blink::Visitor* visitor) override;

 private:
  static ServiceWorkerRegistrationNotifications& From(
      ExecutionContext* context,
      ServiceWorkerRegistration& registration);

  void PrepareShow(mojom::blink::NotificationDataPtr data,
                   ScriptPromiseResolver* resolver);

  void DidLoadResources(scoped_refptr<const SecurityOrigin> origin,
                        mojom::blink::NotificationDataPtr data,
                        ScriptPromiseResolver* resolver,
                        NotificationResourcesLoader* loader);

  Member<ServiceWorkerRegistration> registration_;
  HeapHashSet<Member<NotificationResourcesLoader>> loaders_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistrationNotifications);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NOTIFICATIONS_SERVICE_WORKER_REGISTRATION_NOTIFICATIONS_H_
