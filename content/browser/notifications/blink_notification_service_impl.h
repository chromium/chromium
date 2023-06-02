// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_BLINK_NOTIFICATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_NOTIFICATIONS_BLINK_NOTIFICATION_SERVICE_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/notifications/notification_service.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/gurl.h"

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
      RenderProcessHost* render_process_host,
      const blink::StorageKey& storage_key,
      const GURL& document_url,
      const WeakDocumentPtr& weak_document_ptr,
      RenderProcessHost::NotificationServiceCreatorType creator_type,
      mojo::PendingReceiver<blink::mojom::NotificationService> receiver);

  BlinkNotificationServiceImpl(const BlinkNotificationServiceImpl&) = delete;
  BlinkNotificationServiceImpl& operator=(const BlinkNotificationServiceImpl&) =
      delete;

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

  // Check the permission status for the current `storage_key_`.
  blink::mojom::PermissionStatus CheckPermissionStatus();

  // Validate |notification_data| and |notification_resources| received in a
  // Mojo IPC message. If the validation failed, we'd close the Mojo connection
  // |binding_| and destroy |this| by calling OnConnectionError() directly, then
  // return false. So, please do not touch |this| again after you got a false
  // return value.
  bool ValidateNotificationDataAndResources(
      const blink::PlatformNotificationData& notification_data,
      const blink::NotificationResources& notification_resources);

  void DidWriteNotificationData(DisplayPersistentNotificationCallback callback,
                                bool success,
                                const std::string& notification_id);

  void DidGetNotifications(
      const std::string& filter_tag,
      bool include_triggered,
      GetNotificationsCallback callback,
      bool success,
      const std::vector<NotificationDatabaseData>& notifications);

  // Returns if the current notification service is valid for displaying and
  // handling non persistent notifications.
  // This may report bad message to the receiver and raise connection error if
  // the checks fail.
  bool IsValidForNonPersistentNotification();

  // The notification context that owns this service instance.
  raw_ptr<PlatformNotificationContextImpl, DanglingUntriaged>
      notification_context_;

  raw_ptr<BrowserContext> browser_context_;

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  int render_process_host_id_;

  // The storage key associated with the context that this notification service
  // is communicating with.
  const blink::StorageKey storage_key_;
  // The same as `storage_key_` above but set as if third-party storage
  // partitioning is enabled (regardless of whether it actually is). This is
  // used for collecting metrics when deciding how best to partition
  // notifications.
  const blink::StorageKey storage_key_if_3psp_enabled;

  // The document url that this notification service is communicating with.
  // This is empty when the notification service is created by a worker
  // (including dedicated workers, thus the URL might not be the same as
  // document that `weak_document_ptr_` is pointing at).
  const GURL document_url_;
  // The weak document pointer that this notification service is communicating
  // with. This is either the document that created the notification or the
  // ancestor document of the dedicated worker that created the notification.
  const WeakDocumentPtr weak_document_ptr_;
  // The type of the notification service's creator.
  // See RenderProcessHost::NotificationServiceCreatorType.
  const RenderProcessHost::NotificationServiceCreatorType creator_type_;

  mojo::Receiver<blink::mojom::NotificationService> receiver_;

  base::WeakPtrFactory<BlinkNotificationServiceImpl> weak_factory_for_io_{this};
  base::WeakPtrFactory<BlinkNotificationServiceImpl> weak_factory_for_ui_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_BLINK_NOTIFICATION_SERVICE_IMPL_H_
