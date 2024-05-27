// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/notifications/service_worker_registration_notifications.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_get_notification_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_notification_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/notifications/notification.h"
#include "third_party/blink/renderer/modules/notifications/notification_data.h"
#include "third_party/blink/renderer/modules/notifications/notification_manager.h"
#include "third_party/blink/renderer/modules/notifications/notification_metrics.h"
#include "third_party/blink/renderer/modules/notifications/notification_resources_loader.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

ServiceWorkerRegistrationNotifications::ServiceWorkerRegistrationNotifications(
    ExecutionContext* context,
    ServiceWorkerRegistration* registration)
    : Supplement(*registration), ExecutionContextLifecycleObserver(context) {}

ScriptPromise<IDLUndefined>
ServiceWorkerRegistrationNotifications::showNotification(
    ScriptState* script_state,
    ServiceWorkerRegistration& registration,
    const String& title,
    const NotificationOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsInFencedFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "showNotification() is not allowed in fenced frames.");
    return EmptyPromise();
  }

  // If context object's active worker is null, reject the promise with a
  // TypeError exception.
  if (!registration.active()) {
    RecordPersistentNotificationDisplayResult(
        PersistentNotificationDisplayResult::kRegistrationNotActive);
    exception_state.ThrowTypeError(
        "No active registration available on "
        "the ServiceWorkerRegistration.");
    return EmptyPromise();
  }

  // If permission for notification's origin is not "granted", reject the
  // promise with a TypeError exception, and terminate these substeps.
  if (NotificationManager::From(execution_context)->GetPermissionStatus() !=
      mojom::blink::PermissionStatus::GRANTED) {
    RecordPersistentNotificationDisplayResult(
        PersistentNotificationDisplayResult::kPermissionNotGranted);
    exception_state.ThrowTypeError(
        "No notification permission has been granted for this origin.");
    return EmptyPromise();
  }

  // Validate the developer-provided options to get the NotificationData.
  mojom::blink::NotificationDataPtr data = CreateNotificationData(
      execution_context, title, options, exception_state);
  if (exception_state.HadException())
    return EmptyPromise();

  // Log number of actions developer provided in linear histogram:
  //     0    -> underflow bucket,
  //     1-16 -> distinct buckets,
  //     17+  -> overflow bucket.
  base::UmaHistogramExactLinear(
      "Notifications.PersistentNotificationActionCount",
      options->actions().size(), 17);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  ServiceWorkerRegistrationNotifications::From(execution_context, registration)
      .PrepareShow(std::move(data), resolver);

  return promise;
}

ScriptPromise<IDLSequence<Notification>>
ServiceWorkerRegistrationNotifications::getNotifications(
    ScriptState* script_state,
    ServiceWorkerRegistration& registration,
    const GetNotificationOptions* options) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<Notification>>>(
          script_state);
  auto promise = resolver->Promise();

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  NotificationManager::From(execution_context)
      ->GetNotifications(registration.RegistrationId(), options->tag(),
                         options->includeTriggered(), WrapPersistent(resolver));
  return promise;
}

void ServiceWorkerRegistrationNotifications::ContextDestroyed() {
  for (auto loader : loaders_)
    loader->Stop();
}

void ServiceWorkerRegistrationNotifications::Trace(Visitor* visitor) const {
  visitor->Trace(loaders_);
  Supplement<ServiceWorkerRegistration>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
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
    ScriptPromiseResolver<IDLUndefined>* resolver) {
  scoped_refptr<const SecurityOrigin> origin =
      GetExecutionContext()->GetSecurityOrigin();
  NotificationResourcesLoader* loader =
      MakeGarbageCollected<NotificationResourcesLoader>(WTF::BindOnce(
          &ServiceWorkerRegistrationNotifications::DidLoadResources,
          WrapWeakPersistent(this), std::move(origin), data->Clone(),
          WrapPersistent(resolver)));
  loaders_.insert(loader);
  loader->Start(GetExecutionContext(), *data);
}

void ServiceWorkerRegistrationNotifications::DidLoadResources(
    scoped_refptr<const SecurityOrigin> origin,
    mojom::blink::NotificationDataPtr data,
    ScriptPromiseResolver<IDLUndefined>* resolver,
    NotificationResourcesLoader* loader) {
  DCHECK(loaders_.Contains(loader));

  NotificationManager::From(GetExecutionContext())
      ->DisplayPersistentNotification(GetSupplementable()->RegistrationId(),
                                      std::move(data), loader->GetResources(),
                                      WrapPersistent(resolver));
  loaders_.erase(loader);
}

}  // namespace blink
