// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_platform_notification_service.h"

#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "content/public/common/persistent_notification_status.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"

namespace content {

MockPlatformNotificationService::MockPlatformNotificationService() = default;

MockPlatformNotificationService::~MockPlatformNotificationService() = default;

void MockPlatformNotificationService::DisplayNotification(
    BrowserContext* browser_context,
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
    BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& service_worker_scope,
    const GURL& origin,
    const blink::PlatformNotificationData& notification_data,
    const blink::NotificationResources& notification_resources) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ReplaceNotificationIfNeeded(notification_id);

  PersistentNotification notification;
  notification.browser_context = browser_context;
  notification.origin = origin;

  persistent_notifications_[notification_id] = notification;

  notification_id_map_[base::UTF16ToUTF8(notification_data.title)] =
      notification_id;
}

void MockPlatformNotificationService::CloseNotification(
    BrowserContext* browser_context,
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto non_persistent_iter =
      non_persistent_notifications_.find(notification_id);
  if (non_persistent_iter == non_persistent_notifications_.end())
    return;

  non_persistent_notifications_.erase(non_persistent_iter);
}

void MockPlatformNotificationService::ClosePersistentNotification(
    BrowserContext* browser_context,
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  persistent_notifications_.erase(notification_id);
}

void MockPlatformNotificationService::GetDisplayedNotifications(
    BrowserContext* browser_context,
    const DisplayedNotificationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto displayed_notifications = std::make_unique<std::set<std::string>>();

  for (const auto& kv : persistent_notifications_)
    displayed_notifications->insert(kv.first);
  for (const auto& notification_id : non_persistent_notifications_)
    displayed_notifications->insert(notification_id);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(callback, std::move(displayed_notifications),
                     true /* supports_synchronization */));
}

int64_t MockPlatformNotificationService::ReadNextPersistentNotificationId(
    BrowserContext* browser_context) {
  return ++next_persistent_notification_id_;
}

void MockPlatformNotificationService::RecordNotificationUkmEvent(
    BrowserContext* browser_context,
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

    const PersistentNotification& notification = persistent_iter->second;
    NotificationEventDispatcher::GetInstance()->DispatchNotificationClickEvent(
        notification.browser_context, notification_id, notification.origin,
        action_index, reply, base::DoNothing());
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

  const PersistentNotification& notification = persistent_iter->second;
  NotificationEventDispatcher::GetInstance()->DispatchNotificationCloseEvent(
      notification.browser_context, notification_id, notification.origin,
      by_user, base::DoNothing());
}

void MockPlatformNotificationService::ReplaceNotificationIfNeeded(
    const std::string& notification_id) {
  persistent_notifications_.erase(notification_id);
  non_persistent_notifications_.erase(notification_id);
}

}  // namespace content
