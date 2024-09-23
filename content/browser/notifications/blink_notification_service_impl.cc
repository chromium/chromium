// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/blink_notification_service_impl.h"

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/notifications/notification_event_dispatcher_impl.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/platform_notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/notifications/notification_constants.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace content {

namespace {

const char kBadMessageImproperNotificationImage[] =
    "Received an unexpected message with image while notification images are "
    "disabled.";
const char kBadMessageInvalidNotificationTriggerTimestamp[] =
    "Received an invalid notification trigger timestamp.";
const char kBadMessageInvalidNotificationActionButtons[] =
    "Received a notification with a number of action images that does not "
    "match the number of actions.";
const char kBadMessageNonPersistentNotificationFromServiceWorker[] =
    "Received a non-persistent notification from a service worker.";

bool FilterByTag(const std::string& filter_tag,
                 const NotificationDatabaseData& database_data) {
  // An empty filter tag matches all.
  if (filter_tag.empty())
    return true;
  // Otherwise we need an exact match.
  return filter_tag == database_data.notification_data.tag;
}

bool FilterByTriggered(bool include_triggered,
                       const NotificationDatabaseData& database_data) {
  // Including triggered matches all.
  if (include_triggered)
    return true;
  // Notifications without a trigger always match.
  if (!database_data.notification_data.show_trigger_timestamp)
    return true;
  // Otherwise it has to be triggered already.
  return database_data.has_triggered;
}

// Checks if this notification has a valid trigger.
bool CheckNotificationTriggerRange(
    const blink::PlatformNotificationData& data) {
  if (!data.show_trigger_timestamp)
    return true;

  base::TimeDelta show_trigger_delay =
      data.show_trigger_timestamp.value() - base::Time::Now();

  return show_trigger_delay <= blink::kMaxNotificationShowTriggerDelay;
}

}  // namespace

using blink::mojom::PersistentNotificationError;

BlinkNotificationServiceImpl::BlinkNotificationServiceImpl(
    PlatformNotificationContextImpl* notification_context,
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    RenderProcessHost* render_process_host,
    const blink::StorageKey& storage_key,
    const GURL& document_url,
    const WeakDocumentPtr& weak_document_ptr,
    RenderProcessHost::NotificationServiceCreatorType creator_type,
    mojo::PendingReceiver<blink::mojom::NotificationService> receiver)
    : notification_context_(notification_context),
      browser_context_(browser_context),
      service_worker_context_(std::move(service_worker_context)),
      render_process_host_id_(render_process_host->GetID()),
      storage_key_(storage_key),
      storage_key_if_3psp_enabled(
          storage_key.CopyWithForceEnabledThirdPartyStoragePartitioning()),
      document_url_(document_url),
      weak_document_ptr_(weak_document_ptr),
      creator_type_(creator_type),
      receiver_(this, std::move(receiver)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(notification_context_);
  DCHECK(browser_context_);

  receiver_.set_disconnect_handler(base::BindOnce(
      &BlinkNotificationServiceImpl::OnConnectionError,
      base::Unretained(this) /* the channel is owned by |this| */));
}

BlinkNotificationServiceImpl::~BlinkNotificationServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void BlinkNotificationServiceImpl::GetPermissionStatus(
    GetPermissionStatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_->GetPlatformNotificationService()) {
    std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
    return;
  }

  std::move(callback).Run(CheckPermissionStatus());
}

void BlinkNotificationServiceImpl::OnConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  notification_context_->RemoveService(this);
  // |this| has now been deleted.
}

// Since a non-persistent notification cannot be created by service workers, we
// report the bad message here and raise a connection error.
bool BlinkNotificationServiceImpl::IsValidForNonPersistentNotification() {
  switch (creator_type_) {
    case RenderProcessHost::NotificationServiceCreatorType::kDocument:
    case RenderProcessHost::NotificationServiceCreatorType::kDedicatedWorker:
    case RenderProcessHost::NotificationServiceCreatorType::kSharedWorker:
      return true;
    case RenderProcessHost::NotificationServiceCreatorType::kServiceWorker:
      receiver_.ReportBadMessage(
          kBadMessageNonPersistentNotificationFromServiceWorker);
      OnConnectionError();
      return false;
  }
}

void BlinkNotificationServiceImpl::DisplayNonPersistentNotification(
    const std::string& token,
    const blink::PlatformNotificationData& platform_notification_data,
    const blink::NotificationResources& notification_resources,
    mojo::PendingRemote<blink::mojom::NonPersistentNotificationListener>
        event_listener_remote) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ValidateNotificationDataAndResources(platform_notification_data,
                                            notification_resources))
    return;

  if (!browser_context_->GetPlatformNotificationService())
    return;

  if (CheckPermissionStatus() != blink::mojom::PermissionStatus::GRANTED)
    return;

  if (!IsValidForNonPersistentNotification())
    return;

  base::UmaHistogramBoolean(
      "Notifications.NonPersistentNotificationThirdPartyCount",
      storage_key_if_3psp_enabled.IsThirdPartyContext());

  std::string notification_id =
      notification_context_->notification_id_generator()
          ->GenerateForNonPersistentNotification(storage_key_.origin(), token);

  NotificationEventDispatcherImpl* event_dispatcher =
      NotificationEventDispatcherImpl::GetInstance();
  event_dispatcher->RegisterNonPersistentNotificationListener(
      notification_id, std::move(event_listener_remote), weak_document_ptr_,
      creator_type_);

  browser_context_->GetPlatformNotificationService()->DisplayNotification(
      notification_id, storage_key_.origin().GetURL(), document_url_,
      platform_notification_data, notification_resources);
}

void BlinkNotificationServiceImpl::CloseNonPersistentNotification(
    const std::string& token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_->GetPlatformNotificationService())
    return;

  if (CheckPermissionStatus() != blink::mojom::PermissionStatus::GRANTED)
    return;

  if (!IsValidForNonPersistentNotification())
    return;

  std::string notification_id =
      notification_context_->notification_id_generator()
          ->GenerateForNonPersistentNotification(storage_key_.origin(), token);

  browser_context_->GetPlatformNotificationService()->CloseNotification(
      notification_id);

  // TODO(crbug.com/40398221): Pass a callback here to focus the tab
  // which created the notification, unless the event is canceled.
  NotificationEventDispatcherImpl::GetInstance()
      ->DispatchNonPersistentCloseEvent(notification_id, base::DoNothing());
}

blink::mojom::PermissionStatus
BlinkNotificationServiceImpl::CheckPermissionStatus() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/40637582): It is odd that a service instance can be created
  // for cross-origin subframes, yet the instance is completely oblivious of
  // whether it is serving a top-level browsing context or an embedded one.
  if (creator_type_ ==
      RenderProcessHost::NotificationServiceCreatorType::kDocument) {
    RenderFrameHost* rfh = weak_document_ptr_.AsRenderFrameHostIfValid();
    if (!rfh) {
      return blink::mojom::PermissionStatus::DENIED;
    }
    return browser_context_->GetPermissionController()
        ->GetPermissionStatusForCurrentDocument(
            blink::PermissionType::NOTIFICATIONS, rfh);
  } else {
    RenderProcessHost* rph = RenderProcessHost::FromID(render_process_host_id_);
    if (!rph) {
      return blink::mojom::PermissionStatus::DENIED;
    }
    return browser_context_->GetPermissionController()
        ->GetPermissionStatusForWorker(blink::PermissionType::NOTIFICATIONS,
                                       rph, storage_key_.origin());
  }
}

bool BlinkNotificationServiceImpl::ValidateNotificationDataAndResources(
    const blink::PlatformNotificationData& platform_notification_data,
    const blink::NotificationResources& notification_resources) {
  if (platform_notification_data.actions.size() !=
      notification_resources.action_icons.size()) {
    receiver_.ReportBadMessage(kBadMessageInvalidNotificationActionButtons);
    OnConnectionError();
    return false;
  }

  if (!CheckNotificationTriggerRange(platform_notification_data)) {
    receiver_.ReportBadMessage(kBadMessageInvalidNotificationTriggerTimestamp);
    OnConnectionError();
    return false;
  }

  if (!notification_resources.image.drawsNothing() &&
      !base::FeatureList::IsEnabled(features::kNotificationContentImage)) {
    receiver_.ReportBadMessage(kBadMessageImproperNotificationImage);
    // The above ReportBadMessage() closes |binding_| but does not trigger its
    // connection error handler, so we need to call the error handler explicitly
    // here to do some necessary work.
    OnConnectionError();
    return false;
  }
  return true;
}

void BlinkNotificationServiceImpl::DisplayPersistentNotification(
    int64_t service_worker_registration_id,
    const blink::PlatformNotificationData& platform_notification_data,
    const blink::NotificationResources& notification_resources,
    DisplayPersistentNotificationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The renderer should have checked and disallowed the request for fenced
  // frames and thrown an error in
  // blink::ServiceWorkerRegistrationNotifications. Report a bad message if the
  // renderer if the renderer side check didn't happen for some reason.
  scoped_refptr<ServiceWorkerRegistration> registration =
      service_worker_context_->GetLiveRegistration(
          service_worker_registration_id);
  if (registration && registration->ancestor_frame_type() ==
                          blink::mojom::AncestorFrameType::kFencedFrame) {
    mojo::ReportBadMessage("Notification is not allowed in a fenced frame");
    return;
  }

  if (!ValidateNotificationDataAndResources(platform_notification_data,
                                            notification_resources))
    return;

  if (!browser_context_->GetPlatformNotificationService()) {
    std::move(callback).Run(PersistentNotificationError::INTERNAL_ERROR);
    return;
  }

  if (CheckPermissionStatus() != blink::mojom::PermissionStatus::GRANTED) {
    std::move(callback).Run(PersistentNotificationError::PERMISSION_DENIED);
    return;
  }

  base::UmaHistogramBoolean(
      "Notifications.PersistentNotificationThirdPartyCount",
      storage_key_if_3psp_enabled.IsThirdPartyContext());

  int64_t next_persistent_id =
      browser_context_->GetPlatformNotificationService()
          ->ReadNextPersistentNotificationId();

  NotificationDatabaseData database_data;
  database_data.origin = storage_key_.origin().GetURL();
  database_data.service_worker_registration_id = service_worker_registration_id;
  database_data.notification_data = platform_notification_data;
  database_data.notification_resources = notification_resources;

  // TODO(crbug.com/41405589): Validate resources are not too big (either
  // here or in the mojo struct traits).

  notification_context_->WriteNotificationData(
      next_persistent_id, service_worker_registration_id,
      storage_key_.origin().GetURL(), database_data,
      base::BindOnce(&BlinkNotificationServiceImpl::DidWriteNotificationData,
                     weak_factory_for_ui_.GetWeakPtr(), std::move(callback)));
}

void BlinkNotificationServiceImpl::DidWriteNotificationData(
    DisplayPersistentNotificationCallback callback,
    bool success,
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(success
                              ? PersistentNotificationError::NONE
                              : PersistentNotificationError::INTERNAL_ERROR);
}

void BlinkNotificationServiceImpl::ClosePersistentNotification(
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_->GetPlatformNotificationService())
    return;

  if (CheckPermissionStatus() != blink::mojom::PermissionStatus::GRANTED)
    return;

  notification_context_->DeleteNotificationData(
      notification_id, storage_key_.origin().GetURL(),
      /* close_notification= */ true, base::DoNothing());
}

void BlinkNotificationServiceImpl::GetNotifications(
    int64_t service_worker_registration_id,
    const std::string& filter_tag,
    bool include_triggered,
    GetNotificationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_->GetPlatformNotificationService() ||
      CheckPermissionStatus() != blink::mojom::PermissionStatus::GRANTED) {
    // No permission has been granted for the given origin. It is harmless to
    // try to get notifications without permission, so return empty vectors
    // indicating that no (accessible) notifications exist at this time.
    std::move(callback).Run(std::vector<std::string>(),
                            std::vector<blink::PlatformNotificationData>());
    return;
  }

  auto read_notification_data_callback =
      base::BindOnce(&BlinkNotificationServiceImpl::DidGetNotifications,
                     weak_factory_for_ui_.GetWeakPtr(), filter_tag,
                     include_triggered, std::move(callback));

  notification_context_->ReadAllNotificationDataForServiceWorkerRegistration(
      storage_key_.origin().GetURL(), service_worker_registration_id,
      std::move(read_notification_data_callback));
}

void BlinkNotificationServiceImpl::DidGetNotifications(
    const std::string& filter_tag,
    bool include_triggered,
    GetNotificationsCallback callback,
    bool success,
    const std::vector<NotificationDatabaseData>& notifications) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<std::string> ids;
  std::vector<blink::PlatformNotificationData> datas;

  for (const NotificationDatabaseData& database_data : notifications) {
    if (FilterByTag(filter_tag, database_data) &&
        FilterByTriggered(include_triggered, database_data)) {
      ids.push_back(database_data.notification_id);
      datas.push_back(database_data.notification_data);
    }
  }

  std::move(callback).Run(std::move(ids), std::move(datas));
}

}  // namespace content
