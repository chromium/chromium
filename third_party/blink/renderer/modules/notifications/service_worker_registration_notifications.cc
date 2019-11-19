// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/notifications/service_worker_registration_notifications.h"

#include <utility>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/notifications/get_notification_options.h"
#include "third_party/blink/renderer/modules/notifications/notification_data.h"
#include "third_party/blink/renderer/modules/notifications/notification_manager.h"
#include "third_party/blink/renderer/modules/notifications/notification_options.h"
#include "third_party/blink/renderer/modules/notifications/notification_resources_loader.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

ServiceWorkerRegistrationNotifications::ServiceWorkerRegistrationNotifications(
    ExecutionContext* context,
    ServiceWorkerRegistration* registration)
    : ContextLifecycleObserver(context), registration_(registration) {}

ScriptPromise ServiceWorkerRegistrationNotifications::showNotification(
    ScriptState* script_state,
    ServiceWorkerRegistration& registration,
    const String& title,
    const NotificationOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // If context object's active worker is null, reject the promise with a
  // TypeError exception.
  if (!registration.active()) {
    exception_state.ThrowTypeError(
        "No active registration available on "
        "the ServiceWorkerRegistration.");
    return ScriptPromise();
  }

  // If permission for notification's origin is not "granted", reject the
  // promise with a TypeError exception, and terminate these substeps.
  if (NotificationManager::From(execution_context)->GetPermissionStatus() !=
      mojom::blink::PermissionStatus::GRANTED) {
    exception_state.ThrowTypeError(
        "No notification permission has been granted for this origin.");
    return ScriptPromise();
  }

  // Validate the developer-provided options to get the NotificationData.
  mojom::blink::NotificationDataPtr data = CreateNotificationData(
      execution_context, title, options, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  // Log number of actions developer provided in linear histogram:
  //     0    -> underflow bucket,
  //     1-16 -> distinct buckets,
  //     17+  -> overflow bucket.
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, notification_count_histogram,
      ("Notifications.PersistentNotificationActionCount", 17));
  notification_count_histogram.Count(options->actions().size());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  ServiceWorkerRegistrationNotifications::From(execution_context, registration)
      .PrepareShow(std::move(data), resolver);

  return promise;
}

ScriptPromise ServiceWorkerRegistrationNotifications::getNotifications(
    ScriptState* script_state,
    ServiceWorkerRegistration& registration,
    const GetNotificationOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  NotificationManager::From(execution_context)
      ->GetNotifications(registration.RegistrationId(), options->tag(),
                         options->includeTriggered(), WrapPersistent(resolver));
  return promise;
}

void ServiceWorkerRegistrationNotifications::ContextDestroyed(
    ExecutionContext* context) {
  for (auto loader : loaders_)
    loader->Stop();
}

void ServiceWorkerRegistrationNotifications::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  visitor->Trace(loaders_);
  Supplement<ServiceWorkerRegistration>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

const char ServiceWorkerRegistrationNotifications::kSupplementName[] =
    "ServiceWorkerRegistrationNotifications";

ServiceWorkerRegistrationNotifications&
ServiceWorkerRegistrationNotifications::From(
    ExecutionContext* execution_context,
    ServiceWorkerRegistration& registration) {
  ServiceWorkerRegistrationNotifications* supplement =
      Supplement<ServiceWorkerRegistration>::From<
          ServiceWorkerRegistrationNotifications>(registration);
  if (!supplement) {
    supplement = MakeGarbageCollected<ServiceWorkerRegistrationNotifications>(
        execution_context, &registration);
    ProvideTo(registration, supplement);
  }
  return *supplement;
}

void ServiceWorkerRegistrationNotifications::PrepareShow(
    mojom::blink::NotificationDataPtr data,
    ScriptPromiseResolver* resolver) {
  scoped_refptr<const SecurityOrigin> origin =
      GetExecutionContext()->GetSecurityOrigin();
  NotificationResourcesLoader* loader =
      MakeGarbageCollected<NotificationResourcesLoader>(
          WTF::Bind(&ServiceWorkerRegistrationNotifications::DidLoadResources,
                    WrapWeakPersistent(this), std::move(origin), data->Clone(),
                    WrapPersistent(resolver)));
  loaders_.insert(loader);
  loader->Start(GetExecutionContext(), *data);
}

void ServiceWorkerRegistrationNotifications::DidLoadResources(
    scoped_refptr<const SecurityOrigin> origin,
    mojom::blink::NotificationDataPtr data,
    ScriptPromiseResolver* resolver,
    NotificationResourcesLoader* loader) {
  DCHECK(loaders_.Contains(loader));

  NotificationManager::From(GetExecutionContext())
      ->DisplayPersistentNotification(registration_->RegistrationId(),
                                      std::move(data), loader->GetResources(),
                                      WrapPersistent(resolver));
  loaders_.erase(loader);
}

}  // namespace blink
