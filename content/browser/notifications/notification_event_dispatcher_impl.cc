// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_event_dispatcher_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/notifications/devtools_event_logging.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/persistent_notification_status.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"

namespace content {
namespace {

using NotificationDispatchCompleteCallback =
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
#if defined(OS_ANDROID)
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
      return PersistentNotificationStatus::kServiceWorkerError;
  }
  NOTREACHED();
  return PersistentNotificationStatus::kServiceWorkerError;
}

// To be called when a notification event has finished with a
// blink::ServiceWorkerStatusCode result. Will run or post a task to call
// |dispatch_complete_callback| on the UI thread with a
// PersistentNotificationStatus derived from the service worker status.
void ServiceWorkerNotificationEventFinished(
    NotificationDispatchCompleteCallback dispatch_complete_callback,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(std::move(dispatch_complete_callback),
                     ConvertServiceWorkerStatus(service_worker_status)));
}

// Dispatches the given notification action event on
// |service_worker_registration| if the registration was available. Must be
// called on the service worker core thread.
void DispatchNotificationEventOnRegistration(
    const NotificationDatabaseData& notification_database_data,
    NotificationOperationCallback dispatch_event_action,
    NotificationDispatchCompleteCallback dispatch_complete_callback,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
#if defined(OS_ANDROID)
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
      status = PersistentNotificationStatus::kServiceWorkerError;
      break;
    case blink::ServiceWorkerStatusCode::kOk:
      NOTREACHED();
      break;
  }

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(dispatch_complete_callback), status));
}

// Finds the ServiceWorkerRegistration associated with the |origin| and
// |service_worker_registration_id|. Must be called on the UI thread.
void FindServiceWorkerRegistration(
    const GURL& origin,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    NotificationOperationCallback notification_action_callback,
    NotificationDispatchCompleteCallback dispatch_complete_callback,
    bool success,
    const NotificationDatabaseData& notification_database_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_ANDROID)
  // This LOG(INFO) deliberately exists to help track down the cause of
  // https://crbug.com/534537, where notifications sometimes do not react to
  // the user clicking on them. It should be removed once that's fixed.
  LOG(INFO) << "Lookup for ServiceWoker Registration: success: " << success;
#endif
  if (!success) {
    std::move(dispatch_complete_callback)
        .Run(PersistentNotificationStatus::kDatabaseError);
    return;
  }

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&ServiceWorkerContextWrapper::FindReadyRegistrationForId,
                     service_worker_context,
                     notification_database_data.service_worker_registration_id,
                     origin,
                     base::BindOnce(&DispatchNotificationEventOnRegistration,
                                    notification_database_data,
                                    std::move(notification_action_callback),
                                    std::move(dispatch_complete_callback))));
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
      base::BindOnce(&FindServiceWorkerRegistration, origin,
                     service_worker_context,
                     std::move(notification_read_callback),
                     std::move(dispatch_complete_callback)));
}

// -----------------------------------------------------------------------------

// Dispatches the notificationclick event on |service_worker|.
// Must be called on the service worker core thread.
void DispatchNotificationClickEventOnWorker(
    const scoped_refptr<ServiceWorkerVersion>& service_worker,
    const NotificationDatabaseData& notification_database_data,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    ServiceWorkerVersion::StatusCallback callback,
    blink::ServiceWorkerStatusCode start_worker_status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    BrowserContext* browser_context,
    const ServiceWorkerRegistration* service_worker_registration,
    const NotificationDatabaseData& notification_database_data,
    NotificationDispatchCompleteCallback dispatch_complete_callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&notifications::LogNotificationClickedEventToDevTools,
                     browser_context, notification_database_data, action_index,
                     reply));

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
  std::move(dispatch_complete_callback).Run(status);
}

// Called when the persistent notification close event has been handled
// to remove the notification from the database.
void DeleteNotificationDataFromDatabase(
    const std::string& notification_id,
    const GURL& origin,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    NotificationDispatchCompleteCallback dispatch_complete_callback,
    blink::ServiceWorkerStatusCode status_code) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(
          &PlatformNotificationContext::DeleteNotificationData,
          notification_context, notification_id, origin,
          /* close_notification= */ false,
          base::BindOnce(&OnPersistentNotificationDataDeleted, status_code,
                         std::move(dispatch_complete_callback))));
}

// Dispatches the notificationclose event on |service_worker|.
// Must be called on the service worker core thread.
void DispatchNotificationCloseEventOnWorker(
    const scoped_refptr<ServiceWorkerVersion>& service_worker,
    const NotificationDatabaseData& notification_database_data,
    ServiceWorkerVersion::StatusCallback callback,
    blink::ServiceWorkerStatusCode start_worker_status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
      BrowserContext::GetStoragePartitionForSite(browser_context, origin);

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

void NotificationEventDispatcherImpl::DispatchNotificationClickEvent(
    BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& origin,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    NotificationDispatchCompleteCallback dispatch_complete_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PlatformNotificationContext::Interaction interaction =
      action_index.has_value()
          ? PlatformNotificationContext::Interaction::ACTION_BUTTON_CLICKED
          : PlatformNotificationContext::Interaction::CLICKED;

  DispatchNotificationEvent(
      browser_context, notification_id, origin, interaction,
      base::BindOnce(&DoDispatchNotificationClickEvent, action_index, reply),
      std::move(dispatch_complete_callback));
}

void NotificationEventDispatcherImpl::DispatchNotificationCloseEvent(
    BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& origin,
    bool by_user,
    NotificationDispatchCompleteCallback dispatch_complete_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DispatchNotificationEvent(browser_context, notification_id, origin,
                            PlatformNotificationContext::Interaction::CLOSED,
                            base::BindOnce(&DoDispatchNotificationCloseEvent,
                                           notification_id, by_user),
                            std::move(dispatch_complete_callback));
}

void NotificationEventDispatcherImpl::RegisterNonPersistentNotificationListener(
    const std::string& notification_id,
    mojo::PendingRemote<blink::mojom::NonPersistentNotificationListener>
        event_listener_remote) {
  mojo::Remote<blink::mojom::NonPersistentNotificationListener> bound_remote(
      std::move(event_listener_remote));

  // Observe connection errors, which occur when the JavaScript object or the
  // renderer hosting them goes away. (For example through navigation.) The
  // listener gets freed together with |this|, thus the Unretained is safe.
  bound_remote.set_disconnect_handler(base::BindOnce(
      &NotificationEventDispatcherImpl::
          HandleConnectionErrorForNonPersistentNotificationListener,
      base::Unretained(this), notification_id));

  if (non_persistent_notification_listeners_.count(notification_id)) {
    // Dispatch the close event for any previously displayed notification with
    // the same notification id. This happens whenever a non-persistent
    // notification is replaced (by creating another with the same tag), since
    // from the JavaScript point of view there will be two notification objects,
    // and the old one needs to receive a close event before the new one
    // receives a show event.
    non_persistent_notification_listeners_[notification_id]->OnClose(
        base::DoNothing());
    non_persistent_notification_listeners_.erase(notification_id);
  }

  non_persistent_notification_listeners_.insert(
      {notification_id, std::move(bound_remote)});
}

void NotificationEventDispatcherImpl::DispatchNonPersistentShowEvent(
    const std::string& notification_id) {
  if (!non_persistent_notification_listeners_.count(notification_id))
    return;
  non_persistent_notification_listeners_[notification_id]->OnShow();
}

void NotificationEventDispatcherImpl::DispatchNonPersistentClickEvent(
    const std::string& notification_id,
    NotificationClickEventCallback callback) {
  if (!non_persistent_notification_listeners_.count(notification_id)) {
    std::move(callback).Run(false /* success */);
    return;
  }

  non_persistent_notification_listeners_[notification_id]->OnClick(
      base::BindOnce(std::move(callback), true /* success */));
}

void NotificationEventDispatcherImpl::DispatchNonPersistentCloseEvent(
    const std::string& notification_id,
    base::OnceClosure completed_closure) {
  if (!non_persistent_notification_listeners_.count(notification_id)) {
    std::move(completed_closure).Run();
    return;
  }
  // Listeners get freed together with |this|, thus the Unretained is safe.
  non_persistent_notification_listeners_[notification_id]->OnClose(
      base::BindOnce(
          &NotificationEventDispatcherImpl::OnNonPersistentCloseComplete,
          base::Unretained(this), notification_id,
          std::move(completed_closure)));
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
