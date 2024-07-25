// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PLATFORM_NOTIFICATION_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_PLATFORM_NOTIFICATION_CONTEXT_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/notification_resource_data.h"

class GURL;

namespace blink {
struct NotificationResources;
}  // namespace blink

namespace content {

// Represents the storage context for persistent Web Notifications, specific to
// the storage partition owning the instance. All methods defined in this
// interface may only be used on the UI thread.
class PlatformNotificationContext
    : public base::RefCountedThreadSafe<PlatformNotificationContext,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  using ReadResultCallback =
      base::OnceCallback<void(bool /* success */,
                              const NotificationDatabaseData&)>;

  using ReadResourcesResultCallback =
      base::OnceCallback<void(bool /* success */,
                              const blink::NotificationResources&)>;

  using ReadAllResultCallback =
      base::OnceCallback<void(bool /* success */,
                              const std::vector<NotificationDatabaseData>&)>;

  using WriteResultCallback =
      base::OnceCallback<void(bool /* success */,
                              const std::string& /* notification_id */)>;

  using DeleteResultCallback = base::OnceCallback<void(bool /* success */)>;

  using DeleteAllResultCallback =
      base::OnceCallback<void(bool /* success */, size_t /* deleted_count */)>;

  using WriteResourcesResultCallback =
      base::OnceCallback<void(bool /* success */)>;

  using ReDisplayNotificationsResultCallback =
      base::OnceCallback<void(size_t /* display_count */)>;

  using CountResultCallback =
      base::OnceCallback<void(bool /* success */, int /* count */)>;

  // Reasons for updating a notification, triggering a read.
  enum class Interaction {
    // No interaction was taken with the notification.
    NONE,

    // An action button in the notification was clicked.
    ACTION_BUTTON_CLICKED,

    // The notification itself was clicked.
    CLICKED,

    // The notification was closed.
    CLOSED
  };

  // Reads the data associated with |notification_id| belonging to |origin|
  // from the database. |callback| will be invoked with the success status
  // and a reference to the notification database data when completed.
  // |interaction| is passed in for UKM logging purposes and does not
  // otherwise affect the read.
  virtual void ReadNotificationDataAndRecordInteraction(
      const std::string& notification_id,
      const GURL& origin,
      Interaction interaction,
      ReadResultCallback callback) = 0;

  // Reads the resources associated with |notification_id| belonging to |origin|
  // from the database. |callback| will be invoked with the success status
  // and a reference to the notification resources when completed.
  virtual void ReadNotificationResources(
      const std::string& notification_id,
      const GURL& origin,
      ReadResourcesResultCallback callback) = 0;

  // Reads all data associated with |service_worker_registration_id| belonging
  // to |origin| from the database. |callback| will be invoked with the success
  // status and a vector with all read notification data when completed.
  virtual void ReadAllNotificationDataForServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id,
      ReadAllResultCallback callback) = 0;

  // Counts all currently visible notifications associated with
  // |service_worker_registration_id| belonging to |origin| in the database.
  // |callback| will be invoked with the success status and the count when
  // completed.
  virtual void CountVisibleNotificationsForServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id,
      CountResultCallback callback) = 0;

  // Writes the data associated with a notification to a database and displays
  // it either immediately or at the desired time if the notification has a show
  // trigger defined. When this action is completed, |callback| will be invoked
  // with the success status and the notification id when written successfully.
  // The notification ID field for |database_data| will be generated, and thus
  // must be empty.
  virtual void WriteNotificationData(
      int64_t persistent_notification_id,
      int64_t service_worker_registration_id,
      const GURL& origin,
      const NotificationDatabaseData& database_data,
      WriteResultCallback callback) = 0;

  // Writes the resources passed in |resource_data| to the database. |callback|
  // will be invoked with the success status when the operation has completed.
  virtual void WriteNotificationResources(
      std::vector<NotificationResourceData> resource_data,
      WriteResourcesResultCallback callback) = 0;

  // Displays all notifications that should be on screen for the given
  // |origins|. When this action is completed, |callback| will be invoked with
  // the number of notifications displayed again.
  virtual void ReDisplayNotifications(
      std::vector<GURL> origins,
      ReDisplayNotificationsResultCallback callback) = 0;

  // Deletes all data associated with |notification_id| belonging to |origin|
  // from the database. Closes the notification if |close_notification| is true.
  // |callback| will be invoked with the success status when the operation has
  // completed.
  virtual void DeleteNotificationData(const std::string& notification_id,
                                      const GURL& origin,
                                      bool close_notification,
                                      DeleteResultCallback callback) = 0;

  // Deletes all data of notifications with |tag|, optionally filtered by
  // |is_shown_by_browser|, belonging to |origin| from the database and closes
  // the notifications. |callback| will be invoked with the success status and
  // the number of closed notifications when the operation has completed.
  virtual void DeleteAllNotificationDataWithTag(
      const std::string& tag,
      std::optional<bool> is_shown_by_browser,
      const GURL& origin,
      DeleteAllResultCallback callback) = 0;

  // Checks permissions for all notifications in the database and deletes all
  // that do not have the permission anymore.
  virtual void DeleteAllNotificationDataForBlockedOrigins(
      DeleteAllResultCallback callback) = 0;

  // Trigger all pending notifications.
  virtual void TriggerNotifications() = 0;

 protected:
  friend class base::DeleteHelper<PlatformNotificationContext>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;

  virtual ~PlatformNotificationContext() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PLATFORM_NOTIFICATION_CONTEXT_H_
