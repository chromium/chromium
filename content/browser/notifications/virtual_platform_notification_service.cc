// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/virtual_platform_notification_service.h"

#include <set>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr char kUserDataKey[] = "VirtualPlatformNotificationService";

}  // namespace

VirtualPlatformNotificationService::Notification::Notification() = default;
VirtualPlatformNotificationService::Notification::Notification(
    const Notification&) = default;
VirtualPlatformNotificationService::Notification&
VirtualPlatformNotificationService::Notification::operator=(
    const Notification&) = default;
VirtualPlatformNotificationService::Notification::Notification(Notification&&) =
    default;
VirtualPlatformNotificationService::Notification&
VirtualPlatformNotificationService::Notification::operator=(Notification&&) =
    default;
VirtualPlatformNotificationService::Notification::~Notification() = default;

VirtualPlatformNotificationService::VirtualPlatformNotificationService(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {}

VirtualPlatformNotificationService::~VirtualPlatformNotificationService() {
  base::AutoReset<bool> notifying(&notifying_, true);
  for (Observer& observer : observers_) {
    observer.OnServiceDestroyed(this);
  }
}

// static
VirtualPlatformNotificationService*
VirtualPlatformNotificationService::FromBrowserContext(
    BrowserContext* browser_context) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  return static_cast<VirtualPlatformNotificationService*>(
      browser_context->GetUserData(kUserDataKey));
}

// static
VirtualPlatformNotificationService* VirtualPlatformNotificationService::Attach(
    BrowserContext* browser_context,
    Observer* observer) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* service = FromBrowserContext(browser_context);
  if (!service) {
    service = new VirtualPlatformNotificationService(browser_context);
    browser_context->SetUserData(kUserDataKey, base::WrapUnique(service));
  }
  service->observers_.AddObserver(observer);
  return service;
}

void VirtualPlatformNotificationService::Detach(Observer* observer) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!notifying_);
  observers_.RemoveObserver(observer);
  if (!observers_.empty()) {
    return;
  }
  // Close the remaining notifications like the platform would.
  // TODO(crbug.com/576760752): Deleting the database rows records UKM events,
  // which reach the embedder after this service is destroyed. Skip them.
  auto* dispatcher = NotificationEventDispatcher::GetInstance();
  for (const auto& [id, notification] : notifications_) {
    if (notification.is_persistent) {
      dispatcher->DispatchNotificationCloseEvent(
          browser_context_, id, notification.origin,
          /*by_user=*/false, base::DoNothing());
    } else {
      dispatcher->DispatchNonPersistentCloseEvent(id, base::DoNothing());
    }
  }
  // Destroys `this`.
  browser_context_->RemoveUserData(kUserDataKey);
}

void VirtualPlatformNotificationService::SimulateClick(
    const std::string& notification_id,
    std::optional<int> action_index) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = notifications_.find(notification_id);
  if (it == notifications_.end()) {
    return;
  }
  if (action_index) {
    CHECK(it->second.is_persistent);
    CHECK_GE(*action_index, 0);
    CHECK_LT(static_cast<size_t>(*action_index),
             it->second.data.actions.size());
  }
  auto* dispatcher = NotificationEventDispatcher::GetInstance();
  if (!it->second.is_persistent) {
    dispatcher->DispatchNonPersistentClickEvent(
        notification_id,
        base::BindOnce(
            &VirtualPlatformNotificationService::OnNonPersistentClickDispatched,
            weak_ptr_factory_.GetWeakPtr(), notification_id));
    return;
  }
  const GURL origin = it->second.origin;
  if (browser_context_->GetPermissionController()
          ->GetPermissionResultForOriginWithoutContext(
              PermissionDescriptorUtil::
                  CreatePermissionDescriptorForPermissionType(
                      blink::PermissionType::NOTIFICATIONS),
              url::Origin::Create(origin))
          .status != blink::mojom::PermissionStatus::GRANTED) {
    // Mimic PersistentNotificationHandler: drop the click and close the
    // notification.
    RemoveNotification(notification_id, CloseReason::kOther);
    return;
  }
  dispatcher->DispatchNotificationClickEvent(
      browser_context_, notification_id, origin, action_index,
      /*reply=*/std::nullopt,
      base::BindOnce(
          &VirtualPlatformNotificationService::OnPersistentClickDispatched,
          weak_ptr_factory_.GetWeakPtr(), notification_id));
}

void VirtualPlatformNotificationService::SimulateClose(
    const std::string& notification_id) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = notifications_.find(notification_id);
  if (it == notifications_.end()) {
    return;
  }
  const GURL origin = it->second.origin;
  const bool is_persistent = it->second.is_persistent;
  RemoveNotification(notification_id, CloseReason::kClient);
  auto* dispatcher = NotificationEventDispatcher::GetInstance();
  if (is_persistent) {
    dispatcher->DispatchNotificationCloseEvent(
        browser_context_, notification_id, origin,
        /*by_user=*/true, base::DoNothing());
  } else {
    dispatcher->DispatchNonPersistentCloseEvent(notification_id,
                                                base::DoNothing());
  }
}

void VirtualPlatformNotificationService::DisplayNotification(
    const std::string& notification_id,
    const GURL& origin,
    const GURL& document_url,
    const blink::PlatformNotificationData& notification_data,
    const blink::NotificationResources& notification_resources) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  Notification notification;
  notification.id = notification_id;
  notification.origin = origin;
  notification.data = notification_data;
  AddNotification(std::move(notification));
  NotificationEventDispatcher::GetInstance()->DispatchNonPersistentShowEvent(
      notification_id);
}

void VirtualPlatformNotificationService::DisplayPersistentNotification(
    const std::string& notification_id,
    const GURL& service_worker_scope,
    const GURL& origin,
    const blink::PlatformNotificationData& notification_data,
    const blink::NotificationResources& notification_resources) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  // A tagged notification may replace one that the embedder showed before this
  // service existed. Close that one, which dispatches no close event. If the
  // embedder never showed it, the embedder only keeps the ID as closed until
  // it shows a notification with that ID.
  if (!notification_data.tag.empty() &&
      !notifications_.contains(notification_id)) {
    if (auto* service = browser_context_->GetPlatformNotificationService()) {
      service->ClosePersistentNotification(notification_id);
    }
  }
  Notification notification;
  notification.id = notification_id;
  notification.origin = origin;
  notification.is_persistent = true;
  notification.service_worker_scope = service_worker_scope;
  notification.data = notification_data;
  AddNotification(std::move(notification));
}

void VirtualPlatformNotificationService::CloseNotification(
    const std::string& notification_id) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  if (notifications_.contains(notification_id)) {
    RemoveNotification(notification_id, CloseReason::kOther);
  } else if (auto* service =
                 browser_context_->GetPlatformNotificationService()) {
    // The embedder showed it before this service existed.
    service->CloseNotification(notification_id);
  }
}

void VirtualPlatformNotificationService::ClosePersistentNotification(
    const std::string& notification_id) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  if (notifications_.contains(notification_id)) {
    RemoveNotification(notification_id, CloseReason::kOther);
  } else if (auto* service =
                 browser_context_->GetPlatformNotificationService()) {
    // The embedder showed it before this service existed.
    service->ClosePersistentNotification(notification_id);
  }
}

void VirtualPlatformNotificationService::GetDisplayedNotifications(
    DisplayedNotificationsCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  std::set<std::string> notification_ids;
  for (const auto& [id, notification] : notifications_) {
    notification_ids.insert(id);
  }
  ReplyWithDisplayedNotifications(std::move(notification_ids),
                                  std::move(callback));
}

void VirtualPlatformNotificationService::GetDisplayedNotificationsForOrigin(
    const GURL& origin,
    DisplayedNotificationsCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  std::set<std::string> notification_ids;
  for (const auto& [id, notification] : notifications_) {
    if (url::IsSameOriginWith(notification.origin, origin)) {
      notification_ids.insert(id);
    }
  }
  ReplyWithDisplayedNotifications(std::move(notification_ids),
                                  std::move(callback));
}

void VirtualPlatformNotificationService::ScheduleTrigger(base::Time timestamp) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  // Like MockPlatformNotificationService, fire only triggers that are due.
  // Future triggers are not fired, as in desktop Chrome, where only Android
  // fires them. Notification Triggers is an experimental feature.
  if (timestamp > base::Time::Now()) {
    return;
  }
  browser_context_->ForEachLoadedStoragePartition(
      [](StoragePartition* partition) {
        partition->GetPlatformNotificationContext()->TriggerNotifications();
      });
}

base::Time VirtualPlatformNotificationService::ReadNextTriggerTimestamp() {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  // Future triggers are not tracked, see ScheduleTrigger().
  return base::Time::Max();
}

int64_t VirtualPlatformNotificationService::ReadNextPersistentNotificationId() {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  // Use the embedder's counter if there is one, so that IDs do not collide
  // with notifications already stored in the database.
  if (auto* service = browser_context_->GetPlatformNotificationService()) {
    return service->ReadNextPersistentNotificationId();
  }
  // Otherwise keep a counter that survives the service, so that IDs do not
  // collide with rows written by an earlier service in this process. Rows from
  // earlier browser runs may still collide; an untagged one is then replaced.
  static int64_t next_persistent_notification_id = 1;
  return next_persistent_notification_id++;
}

void VirtualPlatformNotificationService::RecordNotificationUkmEvent(
    const NotificationDatabaseData& data) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
}

void VirtualPlatformNotificationService::AddNotification(
    Notification notification) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!notifying_);
  const std::string id = notification.id;
  auto& stored = notifications_[id] = std::move(notification);
  base::AutoReset<bool> notifying(&notifying_, true);
  for (Observer& observer : observers_) {
    observer.OnNotificationShown(this, stored);
  }
}

void VirtualPlatformNotificationService::RemoveNotification(
    const std::string& notification_id,
    CloseReason reason) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!notifying_);
  // Keep the ID alive, as `notification_id` may refer to the erased key.
  auto node = notifications_.extract(notification_id);
  if (!node) {
    return;
  }
  base::AutoReset<bool> notifying(&notifying_, true);
  for (Observer& observer : observers_) {
    observer.OnNotificationClosed(this, node.key(), reason);
  }
}

void VirtualPlatformNotificationService::ReplyWithDisplayedNotifications(
    std::set<std::string> notification_ids,
    DisplayedNotificationsCallback callback) {
  // Without an embedder service, this service shows all notifications, and the
  // notification database can be synchronized with it. Otherwise, it must not
  // be pruned of notifications that the embedder showed before this service
  // existed.
  GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), std::move(notification_ids),
                         /*supports_synchronization=*/
                         !browser_context_->GetPlatformNotificationService()));
}

void VirtualPlatformNotificationService::OnPersistentClickDispatched(
    const std::string& notification_id,
    PersistentNotificationStatus status) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  // Mimic PersistentNotificationHandler: close the notification on failures
  // that are out of the developer's control.
  switch (status) {
    case PersistentNotificationStatus::kSuccess:
    case PersistentNotificationStatus::kServiceWorkerError:
    case PersistentNotificationStatus::kWaitUntilRejected:
    case PersistentNotificationStatus::kCanceledByAppTerminating:
      break;
    case PersistentNotificationStatus::kServiceWorkerMissing:
    case PersistentNotificationStatus::kDatabaseError:
    case PersistentNotificationStatus::kPermissionMissing:
      RemoveNotification(notification_id, CloseReason::kOther);
      break;
  }
}

void VirtualPlatformNotificationService::OnNonPersistentClickDispatched(
    const std::string& notification_id,
    bool success) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  // Mimic NonPersistentNotificationHandler: close the notification if there
  // was no listener for the click.
  if (!success) {
    RemoveNotification(notification_id, CloseReason::kOther);
  }
}

}  // namespace content
