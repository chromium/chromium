// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_BLINK_NOTIFICATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_NOTIFICATIONS_BLINK_NOTIFICATION_SERVICE_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/notifications/notification_service.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/origin.h"

namespace blink {
struct PlatformNotificationData;
}

namespace content {

struct NotificationDatabaseData;
class PlatformNotificationContextImpl;

// Implementation of the NotificationService used for Web Notifications. Is
// responsible for displaying, updating and reading of both non-persistent
// and persistent notifications. Primarily lives on the UI thread, but jumps to
// the IO thread when needing to interact with the ServiceWorkerContextWrapper.
class CONTENT_EXPORT BlinkNotificationServiceImpl
    : public blink::mojom::NotificationService {
 public:
  BlinkNotificationServiceImpl(
      PlatformNotificationContextImpl* notification_context,
      BrowserContext* browser_context,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::NotificationService> receiver);
  ~BlinkNotificationServiceImpl() override;

  // blink::mojom::NotificationService implementation.
  void GetPermissionStatus(GetPermissionStatusCallback callback) override;
  void DisplayNonPersistentNotification(
      const std::string& token,
      const blink::PlatformNotificationData& platform_notification_data,
      const blink::NotificationResources& notification_resources,
      mojo::PendingRemote<blink::mojom::NonPersistentNotificationListener>
          listener_remote) override;
  void CloseNonPersistentNotification(const std::string& token) override;
  void DisplayPersistentNotification(
      int64_t service_worker_registration_id,
      const blink::PlatformNotificationData& platform_notification_data,
      const blink::NotificationResources& notification_resources,
      DisplayPersistentNotificationCallback) override;
  void ClosePersistentNotification(const std::string& notification_id) override;
  void GetNotifications(int64_t service_worker_registration_id,
                        const std::string& filter_tag,
                        bool include_triggered,
                        GetNotificationsCallback callback) override;

 private:
  // Called when an error is detected on binding_.
  void OnConnectionError();

  // Check the permission status for the current |origin_|.
  blink::mojom::PermissionStatus CheckPermissionStatus();

  // Validate |notification_resources| received in a Mojo IPC message.
  // If the validation failed, we'd close the Mojo connection |binding_| and
  // destroy |this| by calling OnConnectionError() directly, then return false.
  // So, please do not touch |this| again after you got a false return value.
  bool ValidateNotificationResources(
      const blink::NotificationResources& notification_resources);

  // Validate |notification_data| received in a Mojo IPC message.
  // If the validation failed, we'd close the Mojo connection |binding_| and
  // destroy |this| by calling OnConnectionError() directly, then return false.
  // So, please do not touch |this| again after you got a false return value.
  bool ValidateNotificationData(
      const blink::PlatformNotificationData& notification_data);

  void DidWriteNotificationData(DisplayPersistentNotificationCallback callback,
                                bool success,
                                const std::string& notification_id);

  void DidGetNotifications(
      const std::string& filter_tag,
      bool include_triggered,
      GetNotificationsCallback callback,
      bool success,
      const std::vector<NotificationDatabaseData>& notifications);

  // The notification context that owns this service instance.
  PlatformNotificationContextImpl* notification_context_;

  BrowserContext* browser_context_;

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  // The origin that this notification service is communicating with.
  url::Origin origin_;

  mojo::Receiver<blink::mojom::NotificationService> receiver_;

  base::WeakPtrFactory<BlinkNotificationServiceImpl> weak_factory_for_io_{this};
  base::WeakPtrFactory<BlinkNotificationServiceImpl> weak_factory_for_ui_{this};

  DISALLOW_COPY_AND_ASSIGN(BlinkNotificationServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_BLINK_NOTIFICATION_SERVICE_IMPL_H_
