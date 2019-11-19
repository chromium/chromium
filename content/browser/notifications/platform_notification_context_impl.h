// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_CONTEXT_IMPL_H_

#include <stdint.h>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/notifications/notification_database.h"
#include "content/browser/notifications/notification_id_generator.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/platform_notification_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/notifications/notification_service.mojom.h"

class GURL;

namespace base {
class SequencedTaskRunner;
}

namespace url {
class Origin;
}

namespace content {

class BlinkNotificationServiceImpl;
class BrowserContext;
struct NotificationDatabaseData;
class PlatformNotificationServiceProxy;
class ServiceWorkerContextWrapper;

// Implementation of the Web Notification storage context. The public methods
// defined in this interface must only be called on the UI thread.
class CONTENT_EXPORT PlatformNotificationContextImpl
    : public PlatformNotificationContext,
      public ServiceWorkerContextCoreObserver {
 public:
  // Constructs a new platform notification context. If |path| is non-empty, the
  // database will be initialized in the "Platform Notifications" subdirectory
  // of |path|. Otherwise, the database will be initialized in memory.
  PlatformNotificationContextImpl(
      const base::FilePath& path,
      BrowserContext* browser_context,
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context);

  // To be called to initialize the instance.
  void Initialize();

  // To be called when the context is being shut down.
  void Shutdown();

  // Creates a BlinkNotificationServiceImpl that is owned by this context.
  void CreateService(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::NotificationService> receiver);

  // Removes |service| from the list of owned services, for example because the
  // Mojo pipe disconnected. Must be called on the UI thread.
  void RemoveService(BlinkNotificationServiceImpl* service);

  // Returns the notification Id generator owned by the context.
  NotificationIdGenerator* notification_id_generator() {
    return &notification_id_generator_;
  }

  // PlatformNotificationContext implementation.
  void ReadNotificationDataAndRecordInteraction(
      const std::string& notification_id,
      const GURL& origin,
      Interaction interaction,
      ReadResultCallback callback) override;
  void ReadNotificationResources(const std::string& notification_id,
                                 const GURL& origin,
                                 ReadResourcesResultCallback callback) override;
  void WriteNotificationData(int64_t persistent_notification_id,
                             int64_t service_worker_registration_id,
                             const GURL& origin,
                             const NotificationDatabaseData& database_data,
                             WriteResultCallback callback) override;
  void DeleteNotificationData(const std::string& notification_id,
                              const GURL& origin,
                              bool close_notification,
                              DeleteResultCallback callback) override;
  void DeleteAllNotificationDataForBlockedOrigins(
      DeleteAllResultCallback callback) override;
  void ReadAllNotificationDataForServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id,
      ReadAllResultCallback callback) override;
  void TriggerNotifications() override;
  void WriteNotificationResources(
      std::vector<NotificationResourceData> resource_data,
      WriteResourcesResultCallback callback) override;
  void ReDisplayNotifications(
      std::vector<GURL> origins,
      ReDisplayNotificationsResultCallback callback) override;

  // ServiceWorkerContextCoreObserver implementation.
  void OnRegistrationDeleted(int64_t registration_id,
                             const GURL& pattern) override;
  void OnStorageWiped() override;

 private:
  friend class PlatformNotificationContextTest;
  friend class PlatformNotificationContextTriggerTest;

  ~PlatformNotificationContextImpl() override;

  void DidGetNotifications(std::set<std::string> displayed_notifications,
                           bool supports_synchronization);

  using InitializeResultCallback = base::OnceCallback<void(bool)>;

  using ReadAllOriginsResultCallback =
      base::OnceCallback<void(bool /* success */,
                              std::set<GURL> /* origins */)>;

  // Initializes the database if necessary. |callback| will be invoked on the
  // |task_runner_| thread. If everything is available, |callback| will be
  // called with true, otherwise it will be called with false.
  void LazyInitialize(InitializeResultCallback callback);

  // Marks this notification as shown and displays it.
  void DoTriggerNotification(const NotificationDatabaseData& database_data);

  // Opens the database. Must be called on the |task_runner_| thread. |callback|
  // will be invoked on the |task_runner_| thread. When the database has been
  // successfully opened, |callback| will be called with true, otherwise it will
  // be called with false.
  void OpenDatabase(InitializeResultCallback callback);

  // Actually reads the notification data from the database. Must only be
  // called on the |task_runner_| thread. |callback| will be invoked on the
  // UI thread when the operation has completed.
  void DoReadNotificationData(const std::string& notification_id,
                              const GURL& origin,
                              Interaction interaction,
                              ReadResultCallback callback,
                              bool initialized);

  // Actually reads the notification resources from the database. Must only be
  // called on the |task_runner_| thread. |callback| will be invoked on the
  // UI thread when the operation has completed.
  void DoReadNotificationResources(const std::string& notification_id,
                                   const GURL& origin,
                                   ReadResourcesResultCallback callback,
                                   bool initialized);

  // Synchronize displayed notifications. This removes all non-displayed
  // notifications from the database.
  void DoSyncNotificationData(bool supports_synchronization,
                              std::set<std::string> displayed_notifications,
                              bool initialized);

  // Checks if the given notification is still valid, otherwise deletes it from
  // the database.
  void DoHandleSyncNotification(
      bool supports_synchronization,
      const std::set<std::string>& displayed_notifications,
      const NotificationDatabaseData& data);

  // Updates the database (and the result callback) based on
  // |displayed_notifications| if |supports_synchronization|.
  void SynchronizeDisplayedNotificationsForServiceWorkerRegistration(
      base::Time start_time,
      const GURL& origin,
      int64_t service_worker_registration_id,
      ReadAllResultCallback callback,
      std::set<std::string> displayed_notifications,
      bool supports_synchronization);

  // Actually reads all notification data from the database. Must only be
  // called on the |task_runner_| thread. |callback| will be invoked on the
  // UI thread when the operation has completed.
  void DoReadAllNotificationDataForServiceWorkerRegistration(
      base::Time start_time,
      const GURL& origin,
      int64_t service_worker_registration_id,
      ReadAllResultCallback callback,
      std::set<std::string> displayed_notifications,
      bool supports_synchronization,
      bool initialized);

  // Checks if the number of notifications scheduled for |origin| does not
  // exceed the quota.
  bool DoCheckNotificationTriggerQuota(const GURL& origin);

  // Actually writes the notification database to the database. Must only be
  // called on the |task_runner_| thread. |callback| will be invoked on the
  // UI thread when the operation has completed.
  void DoWriteNotificationData(int64_t persistent_notification_id,
                               int64_t service_worker_registration_id,
                               const GURL& origin,
                               const NotificationDatabaseData& database_data,
                               WriteResultCallback callback,
                               bool initialized);

  // Actually deletes the notification information from the database. Must only
  // be called on the |task_runner_| thread. |callback| will be invoked on the
  // UI thread when the operation has completed.
  void DoDeleteNotificationData(const std::string& notification_id,
                                const GURL& origin,
                                DeleteResultCallback callback,
                                bool should_log_close,
                                bool initialized);

  // Actually reads all notification origins from the database. Must only be
  // called on the |task_runner_| thread. |callback| will be invoked on the UI
  // thread when the operation has completed.
  void DoReadAllNotificationOrigins(ReadAllOriginsResultCallback callback,
                                    bool initialized);

  // Checks permissions for all |origins| via PermissionController and deletes
  // all notifications for origins that do not have granted permissions. Must be
  // called on the UI thread. |callback| will be invoked on the UI thread when
  // the operation has completed.
  void CheckPermissionsAndDeleteBlocked(DeleteAllResultCallback callback,
                                        bool success,
                                        std::set<GURL> origins);

  // Actually deletes the notification information from the database. Must only
  // be called on the |task_runner_| thread. |callback| will be invoked on the
  // UI thread when the operation has completed.
  void DoDeleteAllNotificationDataForOrigins(std::set<GURL> origins,
                                             DeleteAllResultCallback callback,
                                             bool initialized);

  // Actually writes the notification resources to the database. Must only be
  // called on the |task_runner_| thread. |callback| will be invoked on the UI
  // thread when the operation has completed.
  void DoWriteNotificationResources(
      std::vector<NotificationResourceData> resource_data,
      WriteResourcesResultCallback callback,
      bool initialized);

  // Actually reads all notification that should be on screen for |origins| from
  // the database and displays them. Must only be called on the |task_runner_|
  // thread. |callback| will be invoked on the UI thread with the number of
  // displayed notifications when the operation has completed.
  void DoReDisplayNotifications(std::vector<GURL> origins,
                                ReDisplayNotificationsResultCallback callback,
                                bool initialized);

  void OnStorageWipedInitialized(bool initialized);

  // Deletes all notifications associated with |service_worker_registration_id|
  // belonging to |origin|. Must be called on the |task_runner_| thread.
  void DoDeleteNotificationsForServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id,
      bool initialized);

  // Destroys the database regardless of its initialization status. This method
  // must only be called on the |task_runner_| thread. Returns if the directory
  // the database was stored in could be emptied.
  bool DestroyDatabase();

  // Returns the path in which the database should be initialized. May be empty.
  base::FilePath GetDatabasePath() const;

  // Sets the task runner to use for testing purposes.
  void SetTaskRunnerForTesting(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  base::FilePath path_;
  BrowserContext* browser_context_;

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<NotificationDatabase> database_;

  NotificationIdGenerator notification_id_generator_;

  // Keeps track of the next trigger timestamp.
  base::Optional<base::Time> next_trigger_;

  // Calls through to PlatformNotificationService methods.
  std::unique_ptr<PlatformNotificationServiceProxy> service_proxy_;

  // The notification services are owned by the platform context, and will be
  // removed when either this class is destroyed or the Mojo pipe disconnects.
  std::vector<std::unique_ptr<BlinkNotificationServiceImpl>> services_;

  NotificationDatabase::UkmCallback ukm_callback_;

  // Flag if the |browser_context_| has been shutdown already.
  bool has_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(PlatformNotificationContextImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_CONTEXT_IMPL_H_
