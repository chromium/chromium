// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/public/browser/platform_notification_service.h"

// The platform notification service is the profile-specific entry point through
// which Web Notifications can be controlled.
// WebEngine does not need to support displaying notifications to the end users,
// the messages instead will be logged for debugging purpose.
class PlatformNotificationServiceImpl final
    : public content::PlatformNotificationService {
 public:
  PlatformNotificationServiceImpl();
  PlatformNotificationServiceImpl(const PlatformNotificationServiceImpl&) =
      delete;
  PlatformNotificationServiceImpl& operator=(
      const PlatformNotificationServiceImpl&) = delete;
  ~PlatformNotificationServiceImpl() override;

  // content::PlatformNotificationService implementation.
  void DisplayNotification(
      const std::string& notification_id,
      const GURL& origin,
      const GURL& document_url,
      const blink::PlatformNotificationData& notification_data,
      const blink::NotificationResources& notification_resources) override;
  void DisplayPersistentNotification(
      const std::string& notification_id,
      const GURL& service_worker_scope,
      const GURL& origin,
      const blink::PlatformNotificationData& notification_data,
      const blink::NotificationResources& notification_resources) override;
  void CloseNotification(const std::string& notification_id) override;
  void ClosePersistentNotification(const std::string& notification_id) override;
  void GetDisplayedNotifications(
      DisplayedNotificationsCallback callback) override;
  void GetDisplayedNotificationsForOrigin(
      const GURL& origin,
      DisplayedNotificationsCallback callback) override;
  void ScheduleTrigger(base::Time timestamp) override;
  base::Time ReadNextTriggerTimestamp() override;
  int64_t ReadNextPersistentNotificationId() override;
  void RecordNotificationUkmEvent(
      const content::NotificationDatabaseData& data) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_
