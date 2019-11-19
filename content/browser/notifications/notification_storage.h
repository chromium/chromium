// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_STORAGE_H_
#define CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_STORAGE_H_

#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/platform_notification_context.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace content {

// The NotificationStorage class is a wrapper around persistent storage, the
// Service Worker database, exposing APIs for read and write queries relating to
// persistent notifications.
class CONTENT_EXPORT NotificationStorage {
 public:
  explicit NotificationStorage(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);
  ~NotificationStorage();

  void WriteNotificationData(
      const NotificationDatabaseData& data,
      PlatformNotificationContext::WriteResultCallback callback);

  void ReadNotificationDataAndRecordInteraction(
      int64_t service_worker_registration_id,
      const std::string& notification_id,
      PlatformNotificationContext::Interaction interaction,
      PlatformNotificationContext::ReadResultCallback callback);

 private:
  void OnWriteComplete(
      const NotificationDatabaseData& data,
      PlatformNotificationContext::WriteResultCallback callback,
      blink::ServiceWorkerStatusCode status);

  void OnReadCompleteUpdateInteraction(
      int64_t service_worker_registration_id,
      PlatformNotificationContext::Interaction interaction,
      PlatformNotificationContext::ReadResultCallback callback,
      const std::vector<std::string>& database_data,
      blink::ServiceWorkerStatusCode status);

  void OnInteractionUpdateComplete(
      std::unique_ptr<NotificationDatabaseData> data,
      PlatformNotificationContext::ReadResultCallback callback,
      blink::ServiceWorkerStatusCode status);

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  base::WeakPtrFactory<NotificationStorage> weak_ptr_factory_{
      this};  // Must be last member.
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_STORAGE_H_
