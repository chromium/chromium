// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/notifications/notification_manager.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_notification_permission.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/notifications/notification.h"
#include "third_party/blink/renderer/modules/notifications/notification_metrics.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
NotificationManager* NotificationManager::From(ExecutionContext* context) {
  DCHECK(context);
  DCHECK(context->IsContextThread());

  NotificationManager* manager =
      Supplement<ExecutionContext>::From<NotificationManager>(context);
  if (!manager) {
    manager = MakeGarbageCollected<NotificationManager>(*context);
    Supplement<ExecutionContext>::ProvideTo(*context, manager);
  }

  return manager;
}

// static
const char NotificationManager::kSupplementName[] = "NotificationManager";

NotificationManager::NotificationManager(ExecutionContext& context)
    : Supplement<ExecutionContext>(context),
      notification_service_(&context),
      permission_service_(&context) {}

NotificationManager::~NotificationManager() = default;

mojom::blink::PermissionStatus NotificationManager::GetPermissionStatus() {
  if (GetSupplementable()->IsContextDestroyed())
    return mojom::blink::PermissionStatus::DENIED;

  // Tentatively have an early return to avoid calling GetNotificationService()
  // during prerendering. The return value is the same as
  // `Notification::permission`'s.
  // TODO(1280155): defer the construction of notification to ensure this method
  // is not called during prerendering instead.
  if (auto* window = DynamicTo<LocalDOMWindow>(GetSupplementable())) {
    if (Document* document = window->document(); document->IsPrerendering()) {
      return mojom::blink::PermissionStatus::ASK;
    }
  }

  SCOPED_UMA_HISTOGRAM_TIMER(
      "Blink.NotificationManager.GetPermissionStatusTime");
  mojom::blink::PermissionStatus permission_status;
  if (!GetNotificationService()->GetPermissionStatus(&permission_status)) {
    // The browser-side Mojo connection was closed, disabling notifications.
    // Hitting this code path means the mojo call is no longer bound to the
    // browser process.
    return mojom::blink::PermissionStatus::DENIED;
  }

  return permission_status;
}

void NotificationManager::GetPermissionStatusAsync(
    base::OnceCallback<void(mojom::blink::PermissionStatus)> callback) {
  if (GetSupplementable()->IsContextDestroyed()) {
    std::move(callback).Run(mojom::blink::PermissionStatus::DENIED);
    return;
  }

  // Tentatively have an early return to avoid calling GetNotificationService()
  // during prerendering. The return value is the same as
  // `Notification::permission`'s.
  // TODO(1280155): defer the construction of notification to ensure this method
  // is not called during prerendering instead.
  if (auto* window = DynamicTo<LocalDOMWindow>(GetSupplementable())) {
    if (Document* document = window->document(); document->IsPrerendering()) {
      std::move(callback).Run(mojom::blink::PermissionStatus::ASK);
      return;
    }
  }

  GetNotificationService()->GetPermissionStatus(std::move(callback));
}

ScriptPromise<V8NotificationPermission> NotificationManager::RequestPermission(
    ScriptState* script_state,
    V8NotificationPermissionCallback* deprecated_callback) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (!permission_service_.is_bound()) {
    // See https://bit.ly/2S0zRAS for task types
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        context->GetTaskRunner(TaskType::kMiscPlatformAPI);
    ConnectToPermissionService(
        context,
        permission_service_.BindNewPipeAndPassReceiver(std::move(task_runner)));
    permission_service_.set_disconnect_handler(
        WTF::BindOnce(&NotificationManager::OnPermissionServiceConnectionError,
                      WrapWeakPersistent(this)));
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8NotificationPermission>>(
          script_state);
  auto promise = resolver->Promise();

  LocalDOMWindow* win = To<LocalDOMWindow>(context);
  permission_service_->RequestPermission(
      CreatePermissionDescriptor(mojom::blink::PermissionName::NOTIFICATIONS),
      LocalFrame::HasTransientUserActivation(win ? win->GetFrame() : nullptr),
      WTF::BindOnce(&NotificationManager::OnPermissionRequestComplete,
                    WrapPersistent(this), WrapPersistent(resolver),
                    WrapPersistent(deprecated_callback)));

  return promise;
}

V8NotificationPermission PermissionStatusToEnum(
    mojom::blink::PermissionStatus permission) {
  switch (permission) {
    case mojom::blink::PermissionStatus::GRANTED:
      return V8NotificationPermission(V8NotificationPermission::Enum::kGranted);
    case mojom::blink::PermissionStatus::DENIED:
      return V8NotificationPermission(V8NotificationPermission::Enum::kDenied);
    case mojom::blink::PermissionStatus::ASK:
      return V8NotificationPermission(V8NotificationPermission::Enum::kDefault);
  }
}

void NotificationManager::OnPermissionRequestComplete(
    ScriptPromiseResolver<V8NotificationPermission>* resolver,
    V8NotificationPermissionCallback* deprecated_callback,
    mojom::blink::PermissionStatus status) {
  V8NotificationPermission permission = PermissionStatusToEnum(status);
  if (deprecated_callback) {
    deprecated_callback->InvokeAndReportException(nullptr, permission);
  }

  resolver->Resolve(permission);
}

void NotificationManager::OnNotificationServiceConnectionError() {
  notification_service_.reset();
}

void NotificationManager::OnPermissionServiceConnectionError() {
  permission_service_.reset();
}

void NotificationManager::DisplayNonPersistentNotification(
    const String& token,
    mojom::blink::NotificationDataPtr notification_data,
    mojom::blink::NotificationResourcesPtr notification_resources,
    mojo::PendingRemote<mojom::blink::NonPersistentNotificationListener>
        event_listener) {
  DCHECK(!token.empty());
  DCHECK(notification_resources);
  GetNotificationService()->DisplayNonPersistentNotification(
      token, std::move(notification_data), std::move(notification_resources),
      std::move(event_listener));
}

void NotificationManager::CloseNonPersistentNotification(const String& token) {
  DCHECK(!token.empty());
  GetNotificationService()->CloseNonPersistentNotification(token);
}

void NotificationManager::DisplayPersistentNotification(
    int64_t service_worker_registration_id,
    mojom::blink::NotificationDataPtr notification_data,
    mojom::blink::NotificationResourcesPtr notification_resources,
    ScriptPromiseResolver<IDLUndefined>* resolver) {
  DCHECK(notification_data);
  DCHECK(notification_resources);
  DCHECK_EQ(notification_data->actions.has_value()
                ? notification_data->actions->size()
                : 0,
            notification_resources->action_icons.has_value()
                ? notification_resources->action_icons->size()
                : 0);

  // Verify that the author-provided payload size does not exceed our limit.
  // This is an implementation-defined limit to prevent abuse of notification
  // data as a storage mechanism. A UMA histogram records the requested sizes,
  // which enables us to track how much data authors are attempting to store.
  //
  // If the size exceeds this limit, reject the showNotification() promise. This
  // is outside of the boundaries set by the specification, but it gives authors
  // an indication that something has gone wrong.
  size_t author_data_size =
      notification_data->data.has_value() ? notification_data->data->size() : 0;

  if (author_data_size >
      mojom::blink::NotificationData::kMaximumDeveloperDataSize) {
    RecordPersistentNotificationDisplayResult(
        PersistentNotificationDisplayResult::kTooMuchData);
    resolver->Reject();
    return;
  }

  GetNotificationService()->DisplayPersistentNotification(
      service_worker_registration_id, std::move(notification_data),
      std::move(notification_resources),
      WTF::BindOnce(&NotificationManager::DidDisplayPersistentNotification,
                    WrapPersistent(this), WrapPersistent(resolver)));
}

void NotificationManager::DidDisplayPersistentNotification(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    mojom::blink::PersistentNotificationError error) {
  switch (error) {
    case mojom::blink::PersistentNotificationError::NONE:
      RecordPersistentNotificationDisplayResult(
          PersistentNotificationDisplayResult::kOk);
      resolver->Resolve();
      return;
    case mojom::blink::PersistentNotificationError::INTERNAL_ERROR:
      RecordPersistentNotificationDisplayResult(
          PersistentNotificationDisplayResult::kInternalError);
      resolver->Reject();
      return;
    case mojom::blink::PersistentNotificationError::PERMISSION_DENIED:
      RecordPersistentNotificationDisplayResult(
          PersistentNotificationDisplayResult::kPermissionDenied);
      // TODO(https://crbug.com/832944): Throw a TypeError if permission denied.
      resolver->Reject();
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void NotificationManager::ClosePersistentNotification(
    const WebString& notification_id) {
  GetNotificationService()->ClosePersistentNotification(notification_id);
}

void NotificationManager::GetNotifications(
    int64_t service_worker_registration_id,
    const WebString& filter_tag,
    bool include_triggered,
    ScriptPromiseResolver<IDLSequence<Notification>>* resolver) {
  GetNotificationService()->GetNotifications(
      service_worker_registration_id, filter_tag, include_triggered,
      WTF::BindOnce(&NotificationManager::DidGetNotifications,
                    WrapPersistent(this), WrapPersistent(resolver)));
}

void NotificationManager::DidGetNotifications(
    ScriptPromiseResolver<IDLSequence<Notification>>* resolver,
    const Vector<String>& notification_ids,
    Vector<mojom::blink::NotificationDataPtr> notification_datas) {
  DCHECK_EQ(notification_ids.size(), notification_datas.size());
  ExecutionContext* context = resolver->GetExecutionContext();
  if (!context)
    return;

  HeapVector<Member<Notification>> notifications;
  notifications.ReserveInitialCapacity(notification_ids.size());

  for (wtf_size_t i = 0; i < notification_ids.size(); ++i) {
    notifications.push_back(Notification::Create(
        context, notification_ids[i], std::move(notification_datas[i]),
        true /* showing */));
  }

  resolver->Resolve(notifications);
}

mojom::blink::NotificationService*
NotificationManager::GetNotificationService() {
  if (!notification_service_.is_bound()) {
    // See https://bit.ly/2S0zRAS for task types
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI);
    GetSupplementable()->GetBrowserInterfaceBroker().GetInterface(
        notification_service_.BindNewPipeAndPassReceiver(task_runner));

    notification_service_.set_disconnect_handler(WTF::BindOnce(
        &NotificationManager::OnNotificationServiceConnectionError,
        WrapWeakPersistent(this)));
  }

  return notification_service_.get();
}

void NotificationManager::Trace(Visitor* visitor) const {
  visitor->Trace(notification_service_);
  visitor->Trace(permission_service_);
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
