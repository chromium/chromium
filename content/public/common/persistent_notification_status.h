// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PERSISTENT_NOTIFICATION_STATUS_H_
#define CONTENT_PUBLIC_COMMON_PERSISTENT_NOTIFICATION_STATUS_H_

namespace content {

// Delivery status for persistent notification clicks to a Service Worker.
// PersistentNotificationStatus entries should not be reordered or removed.
enum class PersistentNotificationStatus {
  // The notificationclick event has been delivered successfully.
  kSuccess = 0,

  // The event could not be delivered because the Service Worker is unavailable.
  kServiceWorkerMissing = 1,

  // The event could not be delivered because of a Service Worker error.
  kServiceWorkerError = 2,

  // The event has been delivered, but the developer extended the event with a
  // promise that has been rejected.
  kWaitUntilRejected = 3,

  // The event could not be delivered because the data associated with the
  // notification could not be read from the database.
  kDatabaseError = 4,

  // The event could not be delivered because no permission had been granted to
  // the origin.
  kPermissionMissing = 5,

  kMaxValue = kPermissionMissing
};

}  // content

#endif  // CONTENT_PUBLIC_COMMON_PERSISTENT_NOTIFICATION_STATUS_H_
