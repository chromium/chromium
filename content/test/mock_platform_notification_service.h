// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_PLATFORM_NOTIFICATION_SERVICE_H_
#define CONTENT_TEST_MOCK_PLATFORM_NOTIFICATION_SERVICE_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/platform_notification_service.h"
#include "url/gurl.h"

namespace blink {
struct NotificationResources;
struct PlatformNotificationData;
}  // namespace blink

namespace content {

class BrowserContext;

// Responsible for tracking active notifications and allowed origins for the
// Web Notification API when running layout and content tests.
class MockPlatformNotificationService : public PlatformNotificationService {
 public:
  MockPlatformNotificationService(BrowserContext* context);

  MockPlatformNotificationService(const MockPlatformNotificationService&) =
      delete;
  MockPlatformNotificationService& operator=(
      const MockPlatformNotificationService&) = delete;

  ~MockPlatformNotificationService() override;

  // Simulates a click on the notification titled |title|. |action_index|
  // indicates which action was clicked. |reply| indicates the user reply.
  // Must be called on the UI thread.
  void SimulateClick(const std::string& title,
                     const std::optional<int>& action_index,
                     const std::optional<std::u16string>& reply);

  // Simulates the closing a notification titled |title|. Must be called on
  // the UI thread.
  void SimulateClose(const std::string& title, bool by_user);

  // PlatformNotificationService implementation.
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
      const NotificationDatabaseData& data) override;

 private:
  // Fakes replacing the notification identified by |notification_id|. Both
  // persistent and non-persistent notifications will be considered for this.
  void ReplaceNotificationIfNeeded(const std::string& notification_id);

  raw_ptr<BrowserContext> context_;

  std::unordered_map<std::string, GURL> persistent_notifications_;
  std::unordered_set<std::string> non_persistent_notifications_;

  // Mapping of titles to notification ids giving test a usable identifier.
  std::unordered_map<std::string, std::string> notification_id_map_;

  int64_t next_persistent_notification_id_ = 1;
};

}  // content

#endif  // CONTENT_TEST_MOCK_PLATFORM_NOTIFICATION_SERVICE_H_
