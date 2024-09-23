// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NOTIFICATION_DATABASE_DATA_H_
#define CONTENT_PUBLIC_BROWSER_NOTIFICATION_DATABASE_DATA_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "url/gurl.h"

namespace content {

// Stores information about a Web Notification as available in the notification
// database. Beyond the notification's own data, its id and attribution need
// to be available for users of the database as well.
// Note: There are extra properties being stored for UKM logging purposes.
// TODO(crbug.com/40576162): Add the UKM that will use these properties.
struct CONTENT_EXPORT NotificationDatabaseData {
  NotificationDatabaseData();
  NotificationDatabaseData(const NotificationDatabaseData& other);
  ~NotificationDatabaseData();

  NotificationDatabaseData& operator=(const NotificationDatabaseData& other);

  // Corresponds to why a notification was closed.
  enum class ClosedReason {
    // The user explicitly closed the notification.
    USER,

    // The notification was closed by the developer.
    DEVELOPER,

    // The notification was found to be closed in the notification database,
    // but why it was closed was not found.
    UNKNOWN
  };

  // Id of the notification as assigned by the NotificationIdGenerator.
  std::string notification_id;

  // Origin of the website this notification is associated with.
  // TODO(crbug.com/40135949): Consider making |origin| a url::Origin
  // field.
  GURL origin;

  // Id of the Service Worker registration this notification is associated with.
  int64_t service_worker_registration_id = 0;

  // Platform data of the notification that's being stored.
  blink::PlatformNotificationData notification_data;

  // Flag if this notification has been triggered by its |showTrigger|.
  bool has_triggered = false;

  // Notification resources to allow showing scheduled notifications. This is
  // only used to store resources in the NotificationDatabase and is not
  // deserialized when reading from the database.
  std::optional<blink::NotificationResources> notification_resources;

  // Boolean for if this current notification is replacing an existing
  // notification.
  bool replaced_existing_notification = false;

  // Number of clicks on the notification itself, i.e. clicks on the
  // notification that take the user to the website. This excludes action
  // button clicks.
  int num_clicks = 0;

  // Number of action button clicks.
  int num_action_button_clicks = 0;

  // Time the notification was first requested to be shown.
  base::Time creation_time_millis;

  // Amount of time, in ms, between when the notification is shown and the
  // first click.
  std::optional<base::TimeDelta> time_until_first_click_millis;

  // Amount of time, in ms, between when the notification is shown and the
  // last click.
  std::optional<base::TimeDelta> time_until_last_click_millis;

  // Amount of time, in ms, between when the notification is shown and closed.
  std::optional<base::TimeDelta> time_until_close_millis;

  // Why the notification was closed.
  ClosedReason closed_reason = ClosedReason::UNKNOWN;

  // Flag for notifications shown by the browser that should not be visible to
  // the origin when requesting a list of notifications.
  bool is_shown_by_browser = false;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NOTIFICATION_DATABASE_DATA_H_
