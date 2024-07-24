// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_event_dispatcher_impl.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "content/browser/notifications/devtools_event_logging.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/common/persistent_notification_status.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {
namespace {

using NotificationDispatchCompleteCallback =
    base::OnceCallback<void(PersistentNotificationStatus,
                            blink::ServiceWorkerStatusCode)>;
using PersistentNotificationDispatchCompleteCallback =
    base::OnceCallback<void(PersistentNotificationStatus)>;
using NotificationOperationCallback =
    base::OnceCallback<void(const ServiceWorkerRegistration*,
                            const NotificationDatabaseData&,
                            NotificationDispatchCompleteCallback)>;
using NotificationOperationCallbackWithContext =
    base::OnceCallback<void(const scoped_refptr<PlatformNotificationContext>&,
                            BrowserContext* browser_context,
                            const ServiceWorkerRegistration*,
                            const NotificationDatabaseData&,
                            NotificationDispatchCompleteCallback)>;

// Derives a PersistentNotificationStatus from the ServiceWorkerStatusCode.
PersistentNotificationStatus ConvertServiceWorkerStatus(
    blink::ServiceWorkerStatusCode service_worker_status) {
#if BUILDFLAG(IS_ANDROID)
  // This LOG(INFO) deliberately exists to help track down the cause of
  // https://crbug.com/534537, where notifications sometimes do not react to
  // the user clicking on them. It should be removed once that's fixed.
  LOG(INFO) << "The notification event has finished: "
            << blink::ServiceWorkerStatusToString(service_worker_status);
#endif
  switch (service_worker_status) {
    case blink::ServiceWorkerStatusCode::kOk:
      return PersistentNotificationStatus::kSuccess;
    case blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected:
      return PersistentNotificationStatus::kWaitUntilRejected;
    case blink::ServiceWorkerStatusCode::kErrorFailed:
    case blink::ServiceWorkerStatusCode::kErrorAbort:
    case blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorProcessNotFound:
    case blink::ServiceWorkerStatusCode::kErrorNotFound:
    case blink::ServiceWorkerStatusCode::kErrorExists:
    case blink::ServiceWorkerStatusCode::kErrorInstallWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorActivateWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorIpcFailed:
    case blink::ServiceWorkerStatusCode::kErrorNetwork:
    case blink::ServiceWorkerStatusCode::kErrorSecurity:
    case blink::ServiceWorkerStatusCode::kErrorState:
    case blink::ServiceWorkerStatusCode::kErrorTimeout:
    case blink::ServiceWorkerStatusCode::kErrorScriptEvaluateFailed:
    case blink::ServiceWorkerStatusCode::kErrorDiskCache:
    case blink::ServiceWorkerStatusCode::kErrorRedundant:
    case blink::ServiceWorkerStatusCode::kErrorDisallowed:
    case blink::ServiceWorkerStatusCode::kErrorInvalidArguments:
    case blink::ServiceWorkerStatusCode::kErrorStorageDisconnected:
    case blink::ServiceWorkerStatusCode::kErrorStorageDataCorrupted:
      return PersistentNotificationStatus::kServiceWorkerError;
  }
  NOTREACHED_IN_MIGRATION();
  return PersistentNotificationStatus::kServiceWorkerError;
}

// To be called when a notification event has finished with a
// blink::ServiceWorkerStatusCode result. Will run or post a task to call
// |dispatch_complete_callback| on the UI thread with a
// PersistentNotificationStatus derived from the service worker status.
void ServiceWorkerNotificationEventFinished(
    NotificationDispatchCompleteCallback dispatch_complete_callback,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(dispatch_complete_callback)
      .Run(ConvertServiceWorkerStatus(service_worker_status),
           service_worker_status);
}

// Dispatches the given notification action event on
// |service_worker_registration| if the registration was available.
void DispatchNotificationEventOnRegistration(
    const NotificationDatabaseData& notification_database_data,
    NotificationOperationCallback dispatch_event_action,
    NotificationDispatchCompleteCallback dispatch_complete_callback,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(IS_ANDROID)
  // This LOG(INFO) deliberately exists to help track down the cause of
  // https://crbug.com/534537, where notifications sometimes do not react to
  // the user clicking on them. It should be removed once that's fixed.
  LOG(INFO) << "Trying to dispatch notification for SW with status: "
            << blink::ServiceWorkerStatusToString(service_worker_status);
#endif
  if (service_worker_status == blink::ServiceWorkerStatusCode::kOk) {
    DCHECK(service_worker_registration->active_version());

    std::move(dispatch_event_action)
        .Run(service_worker_registration.get(), notification_database_data,
             std::move(dispatch_complete_callback));
    return;
  }

  PersistentNotificationStatus status = PersistentNotificationStatus::kSuccess;
  switch (service_worker_status) {
    case blink::ServiceWorkerStatusCode::kErrorNotFound:
      status = PersistentNotificationStatus::kServiceWorkerMissing;
      break;
    case blink::ServiceWorkerStatusCode::kErrorFailed:
    case blink::ServiceWorkerStatusCode::kErrorAbort:
    case blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorProcessNotFound:
    case blink::ServiceWorkerStatusCode::kErrorExists:
    case blink::ServiceWorkerStatusCode::kErrorInstallWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorActivateWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorIpcFailed:
    case blink::ServiceWorkerStatusCode::kErrorNetwork:
    case blink::ServiceWorkerStatusCode::kErrorSecurity:
    case blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected:
    case blink::ServiceWorkerStatusCode::kErrorState:
    case blink::ServiceWorkerStatusCode::kErrorTimeout:
    case blink::ServiceWorkerStatusCode::kErrorScriptEvaluateFailed:
    case blink::ServiceWorkerStatusCode::kErrorDiskCache:
    case blink::ServiceWorkerStatusCode::kErrorRedundant:
    case blink::ServiceWorkerStatusCode::kErrorDisallowed:
    case blink::ServiceWorkerStatusCode::kErrorInvalidArguments:
    case blink::ServiceWorkerStatusCode::kErrorStorageDisconnected:
    case blink::ServiceWorkerStatusCode::kErrorStorageDataCorrupted:
      status = PersistentNotificationStatus::kServiceWorkerError;
      break;
    case blink::ServiceWorkerStatusCode::kOk:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(dispatch_complete_callback), status,
                                service_worker_status));
}

// Finds the ServiceWorkerRegistration associated with the |origin| and
// |service_worker_registration_id|. Must be called on the UI thread.
void FindServiceWorkerRegistration(
    const url::Origin& origin,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    NotificationOperationCallback notification_action_callback,
    NotificationDispatchCompleteCallback dispatch_complete_callback,
    bool success,
    const NotificationDatabaseData& notification_database_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(IS_ANDROID)
  // This LOG(INFO) deliberately exists to help track down the cause of
  // https://crbug.com/534537, where notifications sometimes do not react to
  // the user clicking on them. It should be removed once that's fixed.
  LOG(INFO) << "Lookup for ServiceWoker Registration: success: " << success;
#endif
  if (!success) {
    std::move(dispatch_complete_callback)
        .Run(PersistentNotificationStatus::kDatabaseError,
             blink::ServiceWorkerStatusCode::kOk);
    return;
  }

  // If Push Notification becomes usable from a 3p context then
  // NotificationDatabaseData should be changed to use StorageKey.
  service_worker_context->FindReadyRegistrationForId(
      notification_database_data.service_worker_registration_id,
      blink::StorageKey::CreateFirstParty(origin),
      base::BindOnce(&DispatchNotificationEventOnRegistration,
                     notification_database_data,
                     std::move(notification_action_callback),
                     std::move(dispatch_complete_callback)));
}

// Reads the data associated with the |notification_id| belonging to |origin|
// from the notification context.
void ReadNotificationDatabaseData(
    const std::string& notification_id,
    const GURL& origin,
    PlatformNotificationContext::Interaction interaction,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    NotificationOperationCallback notification_read_callback,
    NotificationDispatchCompleteCallback dispatch_complete_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  notification_context->ReadNotificationDataAndRecordInteraction(
      notification_id, origin, interaction,
      base::BindOnce(&FindServiceWorkerRegistration,
                     url::Origin::Create(origin), service_worker_context,
                     std::move(notification_read_callback),
                     std::move(dispatch_complete_callback)));
}

// -----------------------------------------------------------------------------

// Dispatches the notificationclick event on |service_worker|.
void DispatchNotificationClickEventOnWorker(
    const scoped_refptr<ServiceWorkerVersion>& service_worker,
    const NotificationDatabaseData& notification_database_data,
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply,
    ServiceWorkerVersion::StatusCallback callback,
    blink::ServiceWorkerStatusCode start_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (start_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(start_worker_status);
    return;
  }

  int request_id = service_worker->StartRequest(
      ServiceWorkerMetrics::EventType::NOTIFICATION_CLICK, std::move(callback));

  int action_index_int = -1 /* no value */;
  if (action_index.has_value())
    action_index_int = action_index.value();

  service_worker->endpoint()->DispatchNotificationClickEvent(
      notification_database_data.notification_id,
      notification_database_data.notification_data, action_index_int, reply,
      service_worker->CreateSimpleEventCallback(request_id));
}

// Dispatches the notification click event on the |service_worker_registration|.
void DoDispatchNotificationClickEvent(
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    BrowserContext* browser_context,
    const ServiceWorkerRegistration* service_worker_registration,
    const NotificationDatabaseData& notification_database_data,
    NotificationDispatchCompleteCallback dispatch_complete_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  notifications::LogNotificationClickedEventToDevTools(
      browser_context, notification_database_data, action_index, reply);

  service_worker_registration->active_version()->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::NOTIFICATION_CLICK,
      base::BindOnce(
          &DispatchNotificationClickEventOnWorker,
          base::WrapRefCounted(service_worker_registration->active_version()),
          notification_database_data, action_index, reply,
          base::BindOnce(&ServiceWorkerNotificationEventFinished,
                         std::move(dispatch_complete_callback))));
}

// -----------------------------------------------------------------------------

// Called when the notification data has been deleted to finish the notification
// close event.
void OnPersistentNotificationDataDeleted(
    blink::ServiceWorkerStatusCode service_worker_status,
    NotificationDispatchCompleteCallback dispatch_complete_callback,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PersistentNotificationStatus status =
      success ? PersistentNotificationStatus::kSuccess
              : PersistentNotificationStatus::kDatabaseError;
  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk)
    status = ConvertServiceWorkerStatus(service_worker_status);
  std::move(dispatch_complete_callback).Run(status, service_worker_status);
}

// Called when the persistent notification close event has been handled
// to remove the notification from the database.
void DeleteNotificationDataFromDatabase(
    const std::string& notification_id,
    const GURL& origin,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    NotificationDispatchCompleteCallback dispatch_complete_callback,
    blink::ServiceWorkerStatusCode status_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  notification_context->DeleteNotificationData(
      notification_id, origin,
      /* close_notification= */ false,
      base::BindOnce(&OnPersistentNotificationDataDeleted, status_code,
                     std::move(dispatch_complete_callback)));
}

// Dispatches the notificationclose event on |service_worker|.
void DispatchNotificationCloseEventOnWorker(
    const scoped_refptr<ServiceWorkerVersion>& service_worker,
    const NotificationDatabaseData& notification_database_data,
    ServiceWorkerVersion::StatusCallback callback,
    blink::ServiceWorkerStatusCode start_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (start_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(start_worker_status);
    return;
  }

  int request_id = service_worker->StartRequest(
      ServiceWorkerMetrics::EventType::NOTIFICATION_CLOSE, std::move(callback));

  service_worker->endpoint()->DispatchNotificationCloseEvent(
      notification_database_data.notification_id,
      notification_database_data.notification_data,
      service_worker->CreateSimpleEventCallback(request_id));
}

// Dispatches the notification close event on the service worker registration.
void DoDispatchNotificationCloseEvent(
    const std::string& notification_id,
    bool by_user,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    BrowserContext* browser_context,
    const ServiceWorkerRegistration* service_worker_registration,
    const NotificationDatabaseData& notification_database_data,
    NotificationDispatchCompleteCallback dispatch_complete_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (by_user) {
    service_worker_registration->active_version()->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::NOTIFICATION_CLOSE,
        base::BindOnce(
            &DispatchNotificationCloseEventOnWorker,
            base::WrapRefCounted(service_worker_registration->active_version()),
            notification_database_data,
            base::BindOnce(&DeleteNotificationDataFromDatabase, notification_id,
                           notification_database_data.origin,
                           notification_context,
                           std::move(dispatch_complete_callback))));
  } else {
    DeleteNotificationDataFromDatabase(
        notification_id, notification_database_data.origin,
        notification_context, std::move(dispatch_complete_callback),
        blink::ServiceWorkerStatusCode::kOk);
  }
}

// Dispatches any notification event. The actual, specific event dispatch should
// be done by the |notification_action_callback|.
void DispatchNotificationEvent(
    BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& origin,
    const PlatformNotificationContext::Interaction interaction,
    NotificationOperationCallbackWithContext notification_action_callback,
    NotificationDispatchCompleteCallback dispatch_complete_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!notification_id.empty());
  DCHECK(origin.is_valid());

  StoragePartition* partition =
      browser_context->GetStoragePartitionForUrl(origin);

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  scoped_refptr<PlatformNotificationContext> notification_context =
      partition->GetPlatformNotificationContext();

  ReadNotificationDatabaseData(
      notification_id, origin, interaction, service_worker_context,
      notification_context,
      base::BindOnce(std::move(notification_action_callback),
                     notification_context, browser_context),
      std::move(dispatch_complete_callback));
}

void OnDispatchNotificationClickEventComplete(
    PersistentNotificationDispatchCompleteCallback dispatch_complete_callback,
    PersistentNotificationStatus status,
    blink::ServiceWorkerStatusCode service_worker_status) {
  std::move(dispatch_complete_callback).Run(status);
}

void OnDispatchNotificationCloseEventComplete(
    PersistentNotificationDispatchCompleteCallback dispatch_complete_callback,
    PersistentNotificationStatus status,
    blink::ServiceWorkerStatusCode service_worker_status) {
  std::move(dispatch_complete_callback).Run(status);
}

}  // namespace

// static
NotificationEventDispatcher* NotificationEventDispatcher::GetInstance() {
  return NotificationEventDispatcherImpl::GetInstance();
}

NotificationEventDispatcherImpl*
NotificationEventDispatcherImpl::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::Singleton<NotificationEventDispatcherImpl>::get();
}

NotificationEventDispatcherImpl::NotificationEventDispatcherImpl() = default;
NotificationEventDispatcherImpl::~NotificationEventDispatcherImpl() = default;

NotificationEventDispatcherImpl::NonPersistentNotificationListenerInfo::
    NonPersistentNotificationListenerInfo(
        mojo::Remote<blink::mojom::NonPersistentNotificationListener> remote,
        WeakDocumentPtr document,
        RenderProcessHost::NotificationServiceCreatorType creator_type)
    : remote(std::move(remote)),
      document(document),
      creator_type(creator_type) {}

NotificationEventDispatcherImpl::NonPersistentNotificationListenerInfo::
    NonPersistentNotificationListenerInfo(
        NotificationEventDispatcherImpl::NonPersistentNotificationListenerInfo&&
            info) = default;

NotificationEventDispatcherImpl::NonPersistentNotificationListenerInfo::
    ~NonPersistentNotificationListenerInfo() = default;

base::optional_ref<content::NotificationEventDispatcherImpl::
                       NonPersistentNotificationListenerInfo>
NotificationEventDispatcherImpl::GetListenerIfNotifiable(
    const std::string& notification_id) {
  auto listener = non_persistent_notification_listeners_.find(notification_id);

  // If there is no listener registered for this notification id, no event
  // should be dispatched.
  if (listener == non_persistent_notification_listeners_.end()) {
    return std::nullopt;
  }

  // The non-persistent notification should not be created by service workers.
  DCHECK(listener->second.creator_type !=
         RenderProcessHost::NotificationServiceCreatorType::kServiceWorker);

  RenderFrameHost* rfh = listener->second.document.AsRenderFrameHostIfValid();
  if (!rfh) {
    switch (listener->second.creator_type) {
      case RenderProcessHost::NotificationServiceCreatorType::kDedicatedWorker:
      case RenderProcessHost::NotificationServiceCreatorType::kDocument: {
        // The weak document pointer should be pointing to the document that the
        // notification service is communicating with, if it's empty, it's
        // possible that the document is already destroyed. In this case, the
        // notification event shouldn't be dispatched.
        return std::nullopt;
      }
      case RenderProcessHost::NotificationServiceCreatorType::kSharedWorker: {
        // In this case, the weak document pointer is always null and we
        // shouldn't block the notification.
        return listener->second;
      }
      case RenderProcessHost::NotificationServiceCreatorType::kServiceWorker: {
        NOTREACHED_IN_MIGRATION();
        return std::nullopt;
      }
    }
  }

  // If the associated document is currently in back/forward cache, the
  // function returns nullopt to prevent the listener from being triggered.
  // TODO: in the future, this could be improved to cover more lifecycle
  // state. see: https://crrev.com/c/3861889/comment/e1759c1e_4dd15e4e/
  if (rfh->IsInLifecycleState(
          RenderFrameHost::LifecycleState::kInBackForwardCache)) {
    return std::nullopt;
  }
  return listener->second;
}

void NotificationEventDispatcherImpl::DispatchNotificationClickEvent(
    BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& origin,
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply,
    NotificationDispatchCompleteCallback dispatch_complete_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PlatformNotificationContext::Interaction interaction =
      action_index.has_value()
          ? PlatformNotificationContext::Interaction::ACTION_BUTTON_CLICKED
          : PlatformNotificationContext::Interaction::CLICKED;

  DispatchNotificationEvent(
      browser_context, notification_id, origin, interaction,
      base::BindOnce(&DoDispatchNotificationClickEvent, action_index, reply),
      base::BindOnce(&OnDispatchNotificationClickEventComplete,
                     std::move(dispatch_complete_callback)));
}

void NotificationEventDispatcherImpl::DispatchNotificationCloseEvent(
    BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& origin,
    bool by_user,
    NotificationDispatchCompleteCallback dispatch_complete_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DispatchNotificationEvent(
      browser_context, notification_id, origin,
      PlatformNotificationContext::Interaction::CLOSED,
      base::BindOnce(&DoDispatchNotificationCloseEvent, notification_id,
                     by_user),
      base::BindOnce(&OnDispatchNotificationCloseEventComplete,
                     std::move(dispatch_complete_callback)));
}

void NotificationEventDispatcherImpl::RegisterNonPersistentNotificationListener(
    const std::string& notification_id,
    mojo::PendingRemote<blink::mojom::NonPersistentNotificationListener>
        event_listener_remote,
    const WeakDocumentPtr& event_document_ptr,
    const RenderProcessHost::NotificationServiceCreatorType creator_type) {
  mojo::Remote<blink::mojom::NonPersistentNotificationListener> bound_remote(
      std::move(event_listener_remote));

  // Observe connection errors, which occur when the JavaScript object or the
  // renderer hosting them goes away. (For example through navigation.) The
  // listener gets freed together with |this|, thus the Unretained is safe.
  bound_remote.set_disconnect_handler(base::BindOnce(
      &NotificationEventDispatcherImpl::
          HandleConnectionErrorForNonPersistentNotificationListener,
      base::Unretained(this), notification_id));

  // Dispatch the close event for any previously displayed notification with
  // the same notification id. This happens whenever a non-persistent
  // notification is replaced (by creating another with the same tag), since
  // from the JavaScript point of view there will be two notification objects,
  // and the old one needs to receive a close event before the new one
  // receives a show event.
  DispatchNonPersistentCloseEvent(notification_id, base::DoNothing());

  if (non_persistent_notification_listeners_.count(notification_id)) {
    non_persistent_notification_listeners_.erase(notification_id);
  }
  non_persistent_notification_listeners_.emplace(
      std::piecewise_construct, std::forward_as_tuple(notification_id),
      std::forward_as_tuple(std::move(bound_remote), event_document_ptr,
                            creator_type));
}

// Only fire the notification listeners (including show, click and
// close) when it exists in the map and the document is currently
// not in back/forward cache.
// See https://crbug.com/1350944
void NotificationEventDispatcherImpl::DispatchNonPersistentShowEvent(
    const std::string& notification_id) {
  auto listener = GetListenerIfNotifiable(notification_id);
  if (listener.has_value()) {
    listener->remote->OnShow();
  }
}

void NotificationEventDispatcherImpl::DispatchNonPersistentClickEvent(
    const std::string& notification_id,
    NotificationClickEventCallback callback) {
  auto listener = GetListenerIfNotifiable(notification_id);
  if (listener.has_value()) {
    listener->remote->OnClick(
        base::BindOnce(std::move(callback), true /* success */));
  } else {
    std::move(callback).Run(false /* success */);
  }
}

void NotificationEventDispatcherImpl::DispatchNonPersistentCloseEvent(
    const std::string& notification_id,
    base::OnceClosure completed_closure) {
  auto listener = GetListenerIfNotifiable(notification_id);
  if (listener.has_value()) {
    // Listeners get freed together with `this`, thus the Unretained is safe.
    listener->remote->OnClose(base::BindOnce(
        &NotificationEventDispatcherImpl::OnNonPersistentCloseComplete,
        base::Unretained(this), notification_id, std::move(completed_closure)));
  } else {
    std::move(completed_closure).Run();
  }
}

void NotificationEventDispatcherImpl::OnNonPersistentCloseComplete(
    const std::string& notification_id,
    base::OnceClosure completed_closure) {
  non_persistent_notification_listeners_.erase(notification_id);
  std::move(completed_closure).Run();
}

void NotificationEventDispatcherImpl::
    HandleConnectionErrorForNonPersistentNotificationListener(
        const std::string& notification_id) {
  DCHECK(non_persistent_notification_listeners_.count(notification_id));
  non_persistent_notification_listeners_.erase(notification_id);
}

}  // namespace content
