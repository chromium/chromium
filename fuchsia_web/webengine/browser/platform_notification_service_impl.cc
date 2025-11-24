// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/platform_notification_service_impl.h"

#include <set>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/notimplemented.h"

PlatformNotificationServiceImpl::PlatformNotificationServiceImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PlatformNotificationServiceImpl::~PlatformNotificationServiceImpl() {}

// content::PlatformNotificationService implementation.
void PlatformNotificationServiceImpl::DisplayNotification(
    const std::string& notification_id,
    const GURL& origin,
    const GURL& document_url,
    const blink::PlatformNotificationData& notification_data,
    const blink::NotificationResources& notification_resources) {
  // TODO(crbug.com/424479300): Logging to the dev console may be a better
  // choice.
  LOG(WARNING) << "DisplayNotification " << notification_id << " from "
               << origin << " in " << document_url;
  LOG(WARNING) << notification_data.title << ": " << notification_data.body;
}
void PlatformNotificationServiceImpl::DisplayPersistentNotification(
    const std::string& notification_id,
    const GURL& service_worker_scope,
    const GURL& origin,
    const blink::PlatformNotificationData& notification_data,
    const blink::NotificationResources& notification_resources) {
  DisplayNotification(notification_id, origin, service_worker_scope,
                      notification_data, notification_resources);
}
void PlatformNotificationServiceImpl::CloseNotification(
    const std::string& notification_id) {
  NOTIMPLEMENTED();
}

void PlatformNotificationServiceImpl::ClosePersistentNotification(
    const std::string& notification_id) {
  NOTIMPLEMENTED();
}

void PlatformNotificationServiceImpl::GetDisplayedNotifications(
    DisplayedNotificationsCallback callback) {
  std::move(callback).Run(std::set<std::string>{}, false);
}
void PlatformNotificationServiceImpl::GetDisplayedNotificationsForOrigin(
    const GURL& origin,
    DisplayedNotificationsCallback callback) {
  std::move(callback).Run(std::set<std::string>{}, false);
}

void PlatformNotificationServiceImpl::ScheduleTrigger(base::Time timestamp) {
  NOTIMPLEMENTED();
}

base::Time PlatformNotificationServiceImpl::ReadNextTriggerTimestamp() {
  return base::Time::Max();
}

int64_t PlatformNotificationServiceImpl::ReadNextPersistentNotificationId() {
  // To avoid overwriting data for old notification in the //content-level
  // NotificationDatabase, this function returns a self-incremental integer.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static int64_t id = 0;
  return id++;
}

void PlatformNotificationServiceImpl::RecordNotificationUkmEvent(
    const content::NotificationDatabaseData& data) {
  NOTIMPLEMENTED();
}
