// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/virtual_platform_notification_service.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/task/bind_post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr char kUserDataKey[] = "VirtualPlatformNotificationService";
constexpr char kSkippedUkmEventsKey[] = "VirtualNotificationSkippedUkmEvents";

// IDs of the persistent notifications shown by destroyed services, whose UKM
// events have not arrived yet.
struct SkippedUkmEvents : base::SupportsUserData::Data {
  std::set<std::string> notification_ids;
};

// Returns a callback that runs `callback` on the current sequence, also when
// it is dropped, e.g. with a database task skipped at shutdown on a worker.
base::OnceClosure RunOnCurrentSequenceEvenIfDropped(
    base::OnceClosure callback) {
  return mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

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
  // Close the remaining non-persistent notifications like the platform would.
  // Persistent ones are closed below.
  auto* dispatcher = NotificationEventDispatcher::GetInstance();
  for (const auto& [id, notification] : notifications_) {
    if (!notification.is_persistent) {
      dispatcher->DispatchNonPersistentCloseEvent(id, base::DoNothing());
    }
  }
  // Delete the rows that may still exist: of the persistent notifications still
  // shown, and of removed ones, e.g. after a click without a service worker.
  // Otherwise they would be recorded later, e.g. when a restart prunes them.
  // Rows left at shutdown, when database tasks are skipped, are still recorded
  // then. Skip the UKM events that arrive after `this` is destroyed, unless
  // there is no embedder service, which would not record them anyway.
  SkippedUkmEvents* skipped = nullptr;
  if (browser_context_->GetPlatformNotificationService()) {
    skipped = static_cast<SkippedUkmEvents*>(
        browser_context_->GetUserData(kSkippedUkmEventsKey));
    if (!skipped) {
      auto owned = std::make_unique<SkippedUkmEvents>();
      skipped = owned.get();
      browser_context_->SetUserData(kSkippedUkmEventsKey, std::move(owned));
    }
  }
  for (const auto& [id, origin] : ukm_pending_ids_) {
    if (skipped) {
      skipped->notification_ids.insert(id);
    }
    // Like a close not by the user, without a close event.
    browser_context_->GetStoragePartitionForUrl(origin)
        ->GetPlatformNotificationContext()
        ->DeleteNotificationData(id, origin, /*close_notification=*/false,
                                 base::DoNothing());
  }
  // Destroys `this`.
  browser_context_->RemoveUserData(kUserDataKey);
}

// static
bool VirtualPlatformNotificationService::ConsumeSkippedUkmEvent(
    BrowserContext* browser_context,
    const std::string& notification_id) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* skipped = static_cast<SkippedUkmEvents*>(
      browser_context->GetUserData(kSkippedUkmEventsKey));
  return skipped && skipped->notification_ids.erase(notification_id);
}

const VirtualPlatformNotificationService::Notification*
VirtualPlatformNotificationService::FindNotification(
    const base::UnguessableToken& token) const {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = std::ranges::find(notifications_, token, [](const auto& entry) {
    return entry.second.token;
  });
  return it != notifications_.end() ? &it->second : nullptr;
}

void VirtualPlatformNotificationService::SimulateClick(
    base::UnguessableToken token,
    std::optional<int> action_index,
    base::OnceClosure callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  callback = RunOnCurrentSequenceEvenIfDropped(std::move(callback));
  const Notification* notification = FindNotification(token);
  if (!notification) {
    std::move(callback).Run();
    return;
  }
  if (action_index) {
    CHECK(notification->is_persistent);
    CHECK_GE(*action_index, 0);
    CHECK_LT(static_cast<size_t>(*action_index),
             notification->data.actions.size());
  }
  // Copy, as the notification may be removed below.
  const std::string notification_id = notification->id;
  const GURL origin = notification->origin;
  auto* dispatcher = NotificationEventDispatcher::GetInstance();
  if (!notification->is_persistent) {
    dispatcher->DispatchNonPersistentClickEvent(
        notification_id,
        base::BindOnce(
            &VirtualPlatformNotificationService::OnNonPersistentClickDispatched,
            weak_ptr_factory_.GetWeakPtr(), notification_id)
            .Then(std::move(callback)));
    return;
  }
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
    std::move(callback).Run();
    return;
  }
  dispatcher->DispatchNotificationClickEvent(
      browser_context_, notification_id, origin, action_index,
      /*reply=*/std::nullopt,
      base::BindOnce(
          &VirtualPlatformNotificationService::OnPersistentClickDispatched,
          weak_ptr_factory_.GetWeakPtr(), notification_id)
          .Then(std::move(callback)));
}

void VirtualPlatformNotificationService::SimulateClose(
    base::UnguessableToken token,
    base::OnceClosure callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  callback = RunOnCurrentSequenceEvenIfDropped(std::move(callback));
  const Notification* notification = FindNotification(token);
  if (!notification) {
    std::move(callback).Run();
    return;
  }
  // Copy, as the notification is removed below.
  const std::string notification_id = notification->id;
  const GURL origin = notification->origin;
  const bool is_persistent = notification->is_persistent;
  RemoveNotification(notification_id, CloseReason::kDismissed);
  auto* dispatcher = NotificationEventDispatcher::GetInstance();
  if (is_persistent) {
    dispatcher->DispatchNotificationCloseEvent(
        browser_context_, notification_id, origin,
        /*by_user=*/true,
        base::IgnoreArgs<PersistentNotificationStatus>(std::move(callback)));
  } else {
    dispatcher->DispatchNonPersistentCloseEvent(notification_id,
                                                std::move(callback));
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
  ukm_pending_ids_.insert_or_assign(notification_id, origin);
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
  // Not recorded.
  ukm_pending_ids_.erase(data.notification_id);
}

void VirtualPlatformNotificationService::AddNotification(
    Notification notification) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!notifying_);
  const std::string id = notification.id;
  auto it = notifications_.find(id);
  notification.token = it != notifications_.end()
                           ? it->second.token
                           : base::UnguessableToken::Create();
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
  auto node = notifications_.extract(notification_id);
  if (!node) {
    return;
  }
  base::AutoReset<bool> notifying(&notifying_, true);
  for (Observer& observer : observers_) {
    observer.OnNotificationClosed(this, node.mapped(), reason);
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
