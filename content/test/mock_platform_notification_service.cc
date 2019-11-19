// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_platform_notification_service.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/persistent_notification_status.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"

namespace content {

MockPlatformNotificationService::MockPlatformNotificationService(
    BrowserContext* context)
    : context_(context) {}

MockPlatformNotificationService::~MockPlatformNotificationService() = default;

void MockPlatformNotificationService::DisplayNotification(
    const std::string& notification_id,
    const GURL& origin,
    const blink::PlatformNotificationData& notification_data,
    const blink::NotificationResources& notification_resources) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ReplaceNotificationIfNeeded(notification_id);
  non_persistent_notifications_.insert(notification_id);

  NotificationEventDispatcher::GetInstance()->DispatchNonPersistentShowEvent(
      notification_id);
  notification_id_map_[base::UTF16ToUTF8(notification_data.title)] =
      notification_id;
}

void MockPlatformNotificationService::DisplayPersistentNotification(
    const std::string& notification_id,
    const GURL& service_worker_scope,
    const GURL& origin,
    const blink::PlatformNotificationData& notification_data,
    const blink::NotificationResources& notification_resources) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ReplaceNotificationIfNeeded(notification_id);

  persistent_notifications_[notification_id] = origin;

  notification_id_map_[base::UTF16ToUTF8(notification_data.title)] =
      notification_id;
}

void MockPlatformNotificationService::CloseNotification(
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto non_persistent_iter =
      non_persistent_notifications_.find(notification_id);
  if (non_persistent_iter == non_persistent_notifications_.end())
    return;

  non_persistent_notifications_.erase(non_persistent_iter);
}

void MockPlatformNotificationService::ClosePersistentNotification(
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  persistent_notifications_.erase(notification_id);
}

void MockPlatformNotificationService::GetDisplayedNotifications(
    DisplayedNotificationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::set<std::string> displayed_notifications;

  for (const auto& kv : persistent_notifications_)
    displayed_notifications.insert(kv.first);
  for (const auto& notification_id : non_persistent_notifications_)
    displayed_notifications.insert(notification_id);

  base::PostTask(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(std::move(callback), std::move(displayed_notifications),
                     true /* supports_synchronization */));
}

void MockPlatformNotificationService::ScheduleTrigger(base::Time timestamp) {
  if (timestamp > base::Time::Now())
    return;

  BrowserContext::ForEachStoragePartition(
      context_, base::BindRepeating([](content::StoragePartition* partition) {
        partition->GetPlatformNotificationContext()->TriggerNotifications();
      }));
}

base::Time MockPlatformNotificationService::ReadNextTriggerTimestamp() {
  return base::Time::Max();
}

int64_t MockPlatformNotificationService::ReadNextPersistentNotificationId() {
  return ++next_persistent_notification_id_;
}

void MockPlatformNotificationService::RecordNotificationUkmEvent(
    const NotificationDatabaseData& data) {}

void MockPlatformNotificationService::SimulateClick(
    const std::string& title,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const auto notification_id_iter = notification_id_map_.find(title);
  if (notification_id_iter == notification_id_map_.end())
    return;

  const std::string& notification_id = notification_id_iter->second;

  const auto persistent_iter = persistent_notifications_.find(notification_id);
  const auto non_persistent_iter =
      non_persistent_notifications_.find(notification_id);

  if (persistent_iter != persistent_notifications_.end()) {
    DCHECK(non_persistent_iter == non_persistent_notifications_.end());

    NotificationEventDispatcher::GetInstance()->DispatchNotificationClickEvent(
        context_, notification_id, persistent_iter->second, action_index, reply,
        base::DoNothing());
  } else if (non_persistent_iter != non_persistent_notifications_.end()) {
    DCHECK(!action_index.has_value())
        << "Action buttons are only supported for "
           "persistent notifications";
    NotificationEventDispatcher::GetInstance()->DispatchNonPersistentClickEvent(
        notification_id, base::DoNothing() /* callback */);
  }
}

void MockPlatformNotificationService::SimulateClose(const std::string& title,
                                                    bool by_user) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto notification_id_iter = notification_id_map_.find(title);
  if (notification_id_iter == notification_id_map_.end())
    return;

  const std::string& notification_id = notification_id_iter->second;

  const auto& persistent_iter = persistent_notifications_.find(notification_id);
  if (persistent_iter == persistent_notifications_.end())
    return;

  NotificationEventDispatcher::GetInstance()->DispatchNotificationCloseEvent(
      context_, notification_id, persistent_iter->second, by_user,
      base::DoNothing());
}

void MockPlatformNotificationService::ReplaceNotificationIfNeeded(
    const std::string& notification_id) {
  persistent_notifications_.erase(notification_id);
  non_persistent_notifications_.erase(notification_id);
}

}  // namespace content
