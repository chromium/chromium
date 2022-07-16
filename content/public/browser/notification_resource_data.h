// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NOTIFICATION_RESOURCE_DATA_H_
#define CONTENT_PUBLIC_BROWSER_NOTIFICATION_RESOURCE_DATA_H_

#include <string>

#include "content/common/content_export.h"

class GURL;

namespace blink {
struct NotificationResources;
}  // namespace blink

namespace content {

// Contains the |resources| for a notification with |notification_id| and
// |origin|. Used to pass multiple resources to the PlatformNotificationContext.
struct CONTENT_EXPORT NotificationResourceData {
  NotificationResourceData(const NotificationResourceData&) = delete;
  NotificationResourceData& operator=(const NotificationResourceData&) = delete;

  NotificationResourceData(NotificationResourceData&& data) = default;

  // Id of the notification.
  std::string notification_id;

  // Origin of the website this notification is associated with.
  GURL origin;

  // Notification resources containing all image data.
  blink::NotificationResources resources;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NOTIFICATION_RESOURCE_DATA_H_
