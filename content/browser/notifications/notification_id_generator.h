// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_ID_GENERATOR_H_
#define CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_ID_GENERATOR_H_

#include <stdint.h>

#include <string>
#include <string_view>

#include "content/common/content_export.h"
#include "url/origin.h"

class GURL;

namespace content {

// Generates deterministic notification ids for Web Notifications.
//
// The notification id must be deterministic for a given origin and tag, when
// the tag is non-empty, or unique for the given notification when the tag is
// empty. For non-persistent notifications, the uniqueness will be based on the
// render process id. For persistent notifications, the generated id will be
// globally unique for the lifetime of the notification database.
//
// Notifications coming from the same origin and having the same tag will result
// in the same notification id being generated. This id may then be used to
// update the notification in the platform notification service.
//
// The notification id will be used by the notification service for determining
// when to replace notifications, and as the unique identifier when a
// notification has to be closed programmatically.
//
// It is important to note that, for persistent notifications, the generated
// notification id can outlive the browser process responsible for creating it.
//
// The browser may create notifications on behalf of an origin which will be
// captured as part of the notification id to make sure those ids don't collide
// with ones created via the website.
//
// Note that the PlatformNotificationService is expected to handle
// distinguishing identical generated ids from different browser contexts.
//
// Also note that several functions in NotificationPlatformBridge class
// rely on the format of the notification generated here.
// Code: chrome/android/java/src/org/chromium/chrome/browser/notifications/
// NotificationPlatformBridge.java
class CONTENT_EXPORT NotificationIdGenerator {
 public:
  NotificationIdGenerator() = default;

  NotificationIdGenerator(const NotificationIdGenerator&) = delete;
  NotificationIdGenerator& operator=(const NotificationIdGenerator&) = delete;

  // Returns whether |notification_id| belongs to a persistent notification.
  static bool IsPersistentNotification(const std::string_view& notification_id);

  // Returns whether |notification_id| belongs to a non-persistent notification.
  static bool IsNonPersistentNotification(
      const std::string_view& notification_id);

  // Generates an id for a persistent notification given the notification's
  // origin, tag, is_shown_by_browser and persistent notification id. The
  // persistent notification id will have been created by the persistent
  // notification database.
  std::string GenerateForPersistentNotification(
      const GURL& origin,
      const std::string& tag,
      bool is_shown_by_browser,
      int64_t persistent_notification_id) const;

  // Generates an id for a non-persistent notification given the notification's
  // |origin| and |token|.
  //
  // |token| is what determines which notifications from the same origin receive
  // the same notification ID and therefore which notifications will replace
  // each other. (So different notifications with the same non-empty tag should
  // have the same token, but notifications without tags should have unique
  // tokens.)
  std::string GenerateForNonPersistentNotification(
      const url::Origin& origin,
      const std::string& token) const;
};

}  // namespace context

#endif  // CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_ID_GENERATOR_H_
