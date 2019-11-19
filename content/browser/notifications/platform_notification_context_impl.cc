// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/platform_notification_context_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "content/browser/notifications/blink_notification_service_impl.h"
#include "content/browser/notifications/notification_database.h"
#include "content/browser/notifications/notification_trigger_constants.h"
#include "content/browser/notifications/platform_notification_service_proxy.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/platform_notification_service.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"

namespace content {
namespace {

// Name of the directory in the user's profile directory where the notification
// database files should be stored.
const base::FilePath::CharType kPlatformNotificationsDirectory[] =
    FILE_PATH_LITERAL("Platform Notifications");

// Checks if this notification can trigger in the future.
bool CanTrigger(const NotificationDatabaseData& data) {
  if (!base::FeatureList::IsEnabled(features::kNotificationTriggers))
    return false;
  return data.notification_data.show_trigger_timestamp && !data.has_triggered;
}

void LogNotificationTriggerUMA(const NotificationDatabaseData& data) {
  UMA_HISTOGRAM_BOOLEAN(
      "Notifications.Triggers.HasShowTrigger",
      data.notification_data.show_trigger_timestamp.has_value());

  if (!data.notification_data.show_trigger_timestamp)
    return;

  base::TimeDelta show_trigger_delay =
      data.notification_data.show_trigger_timestamp.value() - base::Time::Now();

  UMA_HISTOGRAM_CUSTOM_COUNTS("Notifications.Triggers.ShowTriggerDelay",
                              show_trigger_delay.InDays(), 1, 365, 50);
}

}  // namespace

PlatformNotificationContextImpl::PlatformNotificationContextImpl(
    const base::FilePath& path,
    BrowserContext* browser_context,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : path_(path),
      browser_context_(browser_context),
      service_worker_context_(service_worker_context),
      has_shutdown_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PlatformNotificationContextImpl::~PlatformNotificationContextImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the database has been initialized, it must be deleted on the task runner
  // thread as closing it may cause file I/O.
  if (database_) {
    DCHECK(task_runner_);
    task_runner_->DeleteSoon(FROM_HERE, database_.release());
  }
}

void PlatformNotificationContextImpl::Initialize() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  service_proxy_ = std::make_unique<PlatformNotificationServiceProxy>(
      service_worker_context_, browser_context_);

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService(
          browser_context_);
  if (!service) {
    std::set<std::string> displayed_notifications;
    DidGetNotifications(std::move(displayed_notifications), false);
    return;
  }

  ukm_callback_ = base::BindRepeating(
      &PlatformNotificationServiceProxy::RecordNotificationUkmEvent,
      service_proxy_->AsWeakPtr());

  service->GetDisplayedNotifications(
      base::BindOnce(&PlatformNotificationContextImpl::DidGetNotifications,
                     this));
}

void PlatformNotificationContextImpl::DidGetNotifications(
    std::set<std::string> displayed_notifications,
    bool supports_synchronization) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Abort if the profile has been shut down already. This mainly happens in
  // tests and very short lived sessions.
  if (has_shutdown_)
    return;

  // Check if there are pending notifications to display.
  base::Time next_trigger = base::Time::Max();
  if (service_proxy_ &&
      base::FeatureList::IsEnabled(features::kNotificationTriggers)) {
    next_trigger = service_proxy_->GetNextTrigger();
  }

  // Synchronize the notifications stored in the database with the set of
  // displaying notifications in |displayed_notifications|. This is necessary
  // because flakiness may cause a platform to inform Chrome of a notification
  // that has since been closed, or because the platform does not support
  // notifications that exceed the lifetime of the browser process.
  if (supports_synchronization || next_trigger <= base::Time::Now()) {
    LazyInitialize(base::BindOnce(
        &PlatformNotificationContextImpl::DoSyncNotificationData, this,
        supports_synchronization, std::move(displayed_notifications)));
  } else if (service_proxy_ && next_trigger != base::Time::Max()) {
    service_proxy_->ScheduleTrigger(next_trigger);
  }

  // |service_worker_context_| may be NULL in tests.
  if (service_worker_context_)
    service_worker_context_->AddObserver(this);
}

void PlatformNotificationContextImpl::DoSyncNotificationData(
    bool supports_synchronization,
    std::set<std::string> displayed_notifications,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized)
    return;

  // Reset |next_trigger_| to keep track of the next trigger timestamp.
  next_trigger_ = base::nullopt;

  // Iterate over all notifications and delete all expired ones.
  NotificationDatabase::Status status =
      database_->ForEachNotificationData(base::BindRepeating(
          &PlatformNotificationContextImpl::DoHandleSyncNotification, this,
          supports_synchronization, displayed_notifications));

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  // Schedule the next trigger timestamp.
  if (next_trigger_ && service_proxy_)
    service_proxy_->ScheduleTrigger(next_trigger_.value());
}

void PlatformNotificationContextImpl::DoHandleSyncNotification(
    bool supports_synchronization,
    const std::set<std::string>& displayed_notifications,
    const NotificationDatabaseData& data) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // Handle pending notifications.
  if (CanTrigger(data)) {
    base::Time timestamp =
        data.notification_data.show_trigger_timestamp.value();
    // Check if we should display this notification.
    if (timestamp <= base::Time::Now())
      DoTriggerNotification(data);
    else if (!next_trigger_ || next_trigger_.value() > timestamp)
      next_trigger_ = timestamp;
    return;
  }

  // Do not delete notifications if the platform does not support syncing them.
  if (!supports_synchronization)
    return;

  // Delete notifications that are not on screen anymore.
  if (!displayed_notifications.count(data.notification_id))
    database_->DeleteNotificationData(data.notification_id, data.origin);
}

void PlatformNotificationContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  has_shutdown_ = true;

  if (service_proxy_)
    service_proxy_->Shutdown();

  services_.clear();

  // |service_worker_context_| may be NULL in tests.
  if (service_worker_context_)
    service_worker_context_->RemoveObserver(this);
}

void PlatformNotificationContextImpl::CreateService(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::NotificationService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  services_.push_back(std::make_unique<BlinkNotificationServiceImpl>(
      this, browser_context_, service_worker_context_, origin,
      std::move(receiver)));
}

void PlatformNotificationContextImpl::RemoveService(
    BlinkNotificationServiceImpl* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::EraseIf(
      services_,
      [service](const std::unique_ptr<BlinkNotificationServiceImpl>& ptr) {
        return ptr.get() == service;
      });
}

void PlatformNotificationContextImpl::
    DeleteAllNotificationDataForBlockedOrigins(
        DeleteAllResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize(base::BindOnce(
      &PlatformNotificationContextImpl::DoReadAllNotificationOrigins, this,
      base::BindOnce(
          &PlatformNotificationContextImpl::CheckPermissionsAndDeleteBlocked,
          this, std::move(callback))));
}

void PlatformNotificationContextImpl::DoReadAllNotificationOrigins(
    ReadAllOriginsResultCallback callback,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  std::set<GURL> origins;
  if (!initialized) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), /* success= */ false,
                                  std::move(origins)));
    return;
  }

  NotificationDatabase::Status status =
      database_->ForEachNotificationData(base::BindRepeating(
          [](std::set<GURL>* origins, const NotificationDatabaseData& data) {
            origins->insert(data.origin);
          },
          &origins));

  bool success = status == NotificationDatabase::STATUS_OK;
  if (!success)
    origins.clear();

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(std::move(callback), success, std::move(origins)));
}

void PlatformNotificationContextImpl::CheckPermissionsAndDeleteBlocked(
    DeleteAllResultCallback callback,
    bool success,
    std::set<GURL> origins) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Make sure |browser_context_| is still valid before getting the controller.
  if (!success || !service_proxy_ || has_shutdown_) {
    std::move(callback).Run(/* success= */ false, /* deleted_count= */ 0);
    return;
  }

  content::PermissionController* controller =
      BrowserContext::GetPermissionController(browser_context_);
  if (!controller) {
    std::move(callback).Run(/* success= */ false, /* deleted_count= */ 0);
    return;
  }

  // Erase all valid origins so we're left with invalid ones.
  base::EraseIf(origins, [controller](const GURL& origin) {
    auto permission = controller->GetPermissionStatus(
        PermissionType::NOTIFICATIONS, origin, origin);
    return permission == blink::mojom::PermissionStatus::GRANTED;
  });

  if (origins.empty()) {
    std::move(callback).Run(/* success= */ true, /* deleted_count= */ 0);
    return;
  }

  LazyInitialize(base::BindOnce(
      &PlatformNotificationContextImpl::DoDeleteAllNotificationDataForOrigins,
      this, std::move(origins), std::move(callback)));
}

void PlatformNotificationContextImpl::DoDeleteAllNotificationDataForOrigins(
    std::set<GURL> origins,
    DeleteAllResultCallback callback,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), /* success= */ false,
                                  /* deleted_count= */ 0));
    return;
  }

  std::set<std::string> deleted_notification_ids;
  NotificationDatabase::Status status = NotificationDatabase::STATUS_OK;
  for (const auto& origin : origins) {
    status = database_->DeleteAllNotificationDataForOrigin(
        origin, /* tag= */ "", &deleted_notification_ids);
    if (status != NotificationDatabase::STATUS_OK)
      break;
  }

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.DeleteAllForOriginsResult",
                            status, NotificationDatabase::STATUS_COUNT);

  bool success = status == NotificationDatabase::STATUS_OK;

  // Blow away the database if deleting data failed due to corruption. Following
  // the contract of the delete methods, consider this to be a success as the
  // caller's goal has been achieved: the data is gone.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED) {
    DestroyDatabase();
    success = true;
  }

  if (service_proxy_) {
    for (const std::string& notification_id : deleted_notification_ids)
      service_proxy_->CloseNotification(notification_id);
  }

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), success,
                                deleted_notification_ids.size()));
}

void PlatformNotificationContextImpl::ReadNotificationDataAndRecordInteraction(
    const std::string& notification_id,
    const GURL& origin,
    const PlatformNotificationContext::Interaction interaction,
    ReadResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize(base::BindOnce(
      &PlatformNotificationContextImpl::DoReadNotificationData, this,
      notification_id, origin, interaction, std::move(callback)));
}

void PlatformNotificationContextImpl::DoReadNotificationData(
    const std::string& notification_id,
    const GURL& origin,
    Interaction interaction,
    ReadResultCallback callback,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), /* success= */ false,
                                  NotificationDatabaseData()));
    return;
  }

  NotificationDatabaseData database_data;
  NotificationDatabase::Status status =
      database_->ReadNotificationDataAndRecordInteraction(
          notification_id, origin, interaction, &database_data);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.ReadResult", status,
                            NotificationDatabase::STATUS_COUNT);

  if (status == NotificationDatabase::STATUS_OK) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), /* success= */ true,
                                  database_data));
    return;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), /* success= */ false,
                                NotificationDatabaseData()));
}

void PlatformNotificationContextImpl::TriggerNotifications() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::set<std::string> displayed_notifications;
  LazyInitialize(base::BindOnce(
      &PlatformNotificationContextImpl::DoSyncNotificationData, this,
      /* supports_synchronization= */ false,
      std::move(displayed_notifications)));
}

void PlatformNotificationContextImpl::DoTriggerNotification(
    const NotificationDatabaseData& database_data) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // Bail out in case we can not display the notification after Shutdown.
  if (!service_proxy_)
    return;

  blink::NotificationResources resources;
  NotificationDatabase::Status status = database_->ReadNotificationResources(
      database_data.notification_id, database_data.origin, &resources);

  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.Database.ReadResourcesForTriggeredResult", status,
      NotificationDatabase::STATUS_COUNT);

  if (status != NotificationDatabase::STATUS_OK)
    resources = blink::NotificationResources();

  // Create a copy of the |database_data| to store the |has_triggered| flag.
  NotificationDatabaseData write_database_data = database_data;
  write_database_data.has_triggered = true;
  status = database_->WriteNotificationData(write_database_data.origin,
                                            write_database_data);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.WriteTriggeredResult",
                            status, NotificationDatabase::STATUS_COUNT);

  if (status != NotificationDatabase::STATUS_OK) {
    database_->DeleteNotificationData(write_database_data.notification_id,
                                      write_database_data.origin);
    return;
  }

  base::Time timestamp =
      database_data.notification_data.show_trigger_timestamp.value();
  UMA_HISTOGRAM_LONG_TIMES("Notifications.Triggers.DisplayDelay",
                           base::Time::Now() - timestamp);

  // Remove resources from DB as we don't need them anymore.
  database_->DeleteNotificationResources(write_database_data.notification_id,
                                         write_database_data.origin);

  write_database_data.notification_resources = std::move(resources);
  service_proxy_->DisplayNotification(std::move(write_database_data),
                                      base::DoNothing());
}

void PlatformNotificationContextImpl::WriteNotificationResources(
    std::vector<NotificationResourceData> resource_data,
    WriteResourcesResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (has_shutdown_ || !service_proxy_)
    return;

  LazyInitialize(base::BindOnce(
      &PlatformNotificationContextImpl::DoWriteNotificationResources, this,
      std::move(resource_data), std::move(callback)));
}

void PlatformNotificationContextImpl::DoWriteNotificationResources(
    std::vector<NotificationResourceData> resource_data,
    WriteResourcesResultCallback callback,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), /* success= */ false));
    return;
  }

  NotificationDatabase::Status status = NotificationDatabase::STATUS_OK;
  for (auto& data : resource_data) {
    NotificationDatabaseData notification_data;
    status = database_->ReadNotificationData(data.notification_id, data.origin,
                                             &notification_data);
    // Ignore missing notifications.
    if (status == NotificationDatabase::STATUS_ERROR_NOT_FOUND) {
      status = NotificationDatabase::STATUS_OK;
      continue;
    }
    if (status != NotificationDatabase::STATUS_OK)
      break;

    // We do not support storing action icons again as they are not used on
    // Android N+ and this will only be used for Q+.
    DCHECK(data.resources.action_icons.empty());
    size_t action_item_count =
        notification_data.notification_data.actions.size();
    data.resources.action_icons.resize(action_item_count);

    notification_data.notification_resources = std::move(data.resources);
    status = database_->WriteNotificationData(data.origin, notification_data);
    if (status != NotificationDatabase::STATUS_OK)
      break;
  }

  if (status == NotificationDatabase::STATUS_OK) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), /* success= */ true));
    return;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), /* success= */ false));
}

void PlatformNotificationContextImpl::ReDisplayNotifications(
    std::vector<GURL> origins,
    ReDisplayNotificationsResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (has_shutdown_ || !service_proxy_)
    return;

  LazyInitialize(
      base::BindOnce(&PlatformNotificationContextImpl::DoReDisplayNotifications,
                     this, std::move(origins), std::move(callback)));
}

void PlatformNotificationContextImpl::DoReDisplayNotifications(
    std::vector<GURL> origins,
    ReDisplayNotificationsResultCallback callback,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  size_t display_count = 0;
  if (!initialized) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), display_count));
    return;
  }

  NotificationDatabase::Status status = NotificationDatabase::STATUS_OK;
  for (const auto& origin : origins) {
    std::vector<NotificationDatabaseData> datas;
    status = database_->ReadAllNotificationDataForOrigin(origin, &datas);
    if (status != NotificationDatabase::STATUS_OK)
      break;

    for (const auto& data : datas) {
      if (CanTrigger(data))
        continue;
      blink::NotificationResources resources;
      status = database_->ReadNotificationResources(data.notification_id,
                                                    data.origin, &resources);
      // Ignore notifications without resources as they might already be shown.
      if (status == NotificationDatabase::STATUS_ERROR_NOT_FOUND) {
        status = NotificationDatabase::STATUS_OK;
        continue;
      }
      if (status != NotificationDatabase::STATUS_OK)
        break;

      // Remove resources from DB as we don't need them anymore.
      database_->DeleteNotificationResources(data.notification_id, data.origin);

      NotificationDatabaseData display_data = data;
      display_data.notification_resources = std::move(resources);
      service_proxy_->DisplayNotification(std::move(display_data),
                                          base::DoNothing());
      ++display_count;
    }
    if (status != NotificationDatabase::STATUS_OK)
      break;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  base::PostTask(FROM_HERE,
                 {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
                 base::BindOnce(std::move(callback), display_count));
}

void PlatformNotificationContextImpl::ReadNotificationResources(
    const std::string& notification_id,
    const GURL& origin,
    ReadResourcesResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize(base::BindOnce(
      &PlatformNotificationContextImpl::DoReadNotificationResources, this,
      notification_id, origin, std::move(callback)));
}

void PlatformNotificationContextImpl::DoReadNotificationResources(
    const std::string& notification_id,
    const GURL& origin,
    ReadResourcesResultCallback callback,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), /* success= */ false,
                                  blink::NotificationResources()));
    return;
  }

  blink::NotificationResources notification_resources;
  NotificationDatabase::Status status = database_->ReadNotificationResources(
      notification_id, origin, &notification_resources);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.ReadResourcesResult",
                            status, NotificationDatabase::STATUS_COUNT);

  if (status == NotificationDatabase::STATUS_OK) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), /* success= */ true,
                                  notification_resources));
    return;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), /* success= */ false,
                                blink::NotificationResources()));
}

void PlatformNotificationContextImpl::
    SynchronizeDisplayedNotificationsForServiceWorkerRegistration(
        base::Time start_time,
        const GURL& origin,
        int64_t service_worker_registration_id,
        ReadAllResultCallback callback,
        std::set<std::string> notification_ids,
        bool supports_synchronization) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize(
      base::BindOnce(&PlatformNotificationContextImpl::
                         DoReadAllNotificationDataForServiceWorkerRegistration,
                     this, start_time, origin, service_worker_registration_id,
                     std::move(callback), std::move(notification_ids),
                     supports_synchronization));
}

void PlatformNotificationContextImpl::
    ReadAllNotificationDataForServiceWorkerRegistration(
        const GURL& origin,
        int64_t service_worker_registration_id,
        ReadAllResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService(
          browser_context_);

  if (!service) {
    // Rely on the database only
    std::set<std::string> notification_ids;
    SynchronizeDisplayedNotificationsForServiceWorkerRegistration(
        base::Time::Now(), origin, service_worker_registration_id,
        std::move(callback), std::move(notification_ids),
        /* supports_synchronization= */ false);
    return;
  }

  service->GetDisplayedNotifications(base::BindOnce(
      &PlatformNotificationContextImpl::
          SynchronizeDisplayedNotificationsForServiceWorkerRegistration,
      this, base::Time::Now(), origin, service_worker_registration_id,
      std::move(callback)));
}

void PlatformNotificationContextImpl::
    DoReadAllNotificationDataForServiceWorkerRegistration(
        base::Time start_time,
        const GURL& origin,
        int64_t service_worker_registration_id,
        ReadAllResultCallback callback,
        std::set<std::string> displayed_notifications,
        bool supports_synchronization,
        bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), /* success= */ false,
                                  std::vector<NotificationDatabaseData>()));
    return;
  }

  std::vector<NotificationDatabaseData> notification_datas;

  NotificationDatabase::Status status =
      database_->ReadAllNotificationDataForServiceWorkerRegistration(
          origin, service_worker_registration_id, &notification_datas);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.ReadForServiceWorkerResult",
                            status, NotificationDatabase::STATUS_COUNT);

  std::vector<std::string> obsolete_notifications;

  if (status == NotificationDatabase::STATUS_OK) {
    if (supports_synchronization) {
      for (auto it = notification_datas.begin();
           it != notification_datas.end();) {
        // The database is only used for persistent notifications.
        DCHECK(NotificationIdGenerator::IsPersistentNotification(
            it->notification_id));
        if (displayed_notifications.count(it->notification_id) ||
            CanTrigger(*it) || it->creation_time_millis >= start_time) {
          ++it;
        } else {
          obsolete_notifications.push_back(it->notification_id);
          it = notification_datas.erase(it);
        }
      }
    }

    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), /* success= */ true,
                                  notification_datas));

    // Remove notifications that are not actually on display anymore.
    for (const auto& it : obsolete_notifications)
      database_->DeleteNotificationData(it, origin);
    return;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), /* success= */ false,
                                std::vector<NotificationDatabaseData>()));
}

void PlatformNotificationContextImpl::WriteNotificationData(
    int64_t persistent_notification_id,
    int64_t service_worker_registration_id,
    const GURL& origin,
    const NotificationDatabaseData& database_data,
    WriteResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize(base::BindOnce(
      &PlatformNotificationContextImpl::DoWriteNotificationData, this,
      service_worker_registration_id, persistent_notification_id, origin,
      database_data, std::move(callback)));
}

bool PlatformNotificationContextImpl::DoCheckNotificationTriggerQuota(
    const GURL& origin) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  int notification_count = 0;
  // Iterate over all notifications and count all scheduled notifications for
  // |origin|.
  NotificationDatabase::Status status =
      database_->ForEachNotificationData(base::BindRepeating(
          [](const GURL& expected_origin, int* count,
             const NotificationDatabaseData& data) {
            if (CanTrigger(data) && data.origin == expected_origin)
              *count = *count + 1;
          },
          origin, &notification_count));

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  return notification_count < kMaximumScheduledNotificationsPerOrigin;
}

void PlatformNotificationContextImpl::DoWriteNotificationData(
    int64_t service_worker_registration_id,
    int64_t persistent_notification_id,
    const GURL& origin,
    const NotificationDatabaseData& database_data,
    WriteResultCallback callback,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(database_data.notification_id.empty());
  if (!initialized || !service_proxy_) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), /* success= */ false,
                                  /* notification_id= */ ""));
    return;
  }

  if (base::FeatureList::IsEnabled(features::kNotificationTriggers))
    LogNotificationTriggerUMA(database_data);

  bool replaces_existing = false;
  std::string notification_id =
      notification_id_generator_.GenerateForPersistentNotification(
          origin, database_data.notification_data.tag,
          persistent_notification_id);

  // Eagerly delete data for replaced notifications from the database.
  if (!database_data.notification_data.tag.empty()) {
    std::set<std::string> deleted_notification_ids;
    NotificationDatabase::Status delete_status =
        database_->DeleteAllNotificationDataForOrigin(
            origin, database_data.notification_data.tag,
            &deleted_notification_ids);

    replaces_existing = deleted_notification_ids.count(notification_id) != 0;

    UMA_HISTOGRAM_ENUMERATION("Notifications.Database.DeleteBeforeWriteResult",
                              delete_status,
                              NotificationDatabase::STATUS_COUNT);

    // Unless the database was corrupted following this change, there is no
    // reason to bail out here in event of failure because the notification
    // display logic will handle notification replacement for the user.
    if (delete_status == NotificationDatabase::STATUS_ERROR_CORRUPTED) {
      DestroyDatabase();

      base::PostTask(FROM_HERE, {BrowserThread::UI},
                     base::BindOnce(std::move(callback), /* success= */ false,
                                    /* notification_id= */ ""));
      return;
    }
  }

  // Create a copy of the |database_data| to store a generated notification ID.
  NotificationDatabaseData write_database_data = database_data;
  write_database_data.notification_id = notification_id;
  write_database_data.origin = origin;

  if (CanTrigger(write_database_data) &&
      !DoCheckNotificationTriggerQuota(origin)) {
    // TODO(knollr): Reply with a custom error so developers can handle this.
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), /* success= */ false,
                                  /* notification_id= */ ""));
    return;
  }

  // Only store resources for notifications that will be scheduled.
  if (!CanTrigger(write_database_data))
    write_database_data.notification_resources = base::nullopt;

  NotificationDatabase::Status status =
      database_->WriteNotificationData(origin, write_database_data);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.WriteResult", status,
                            NotificationDatabase::STATUS_COUNT);

  if (status == NotificationDatabase::STATUS_OK) {
    if (CanTrigger(write_database_data)) {
      if (replaces_existing)
        service_proxy_->CloseNotification(notification_id);

      // Schedule notification to be shown.
      service_proxy_->ScheduleNotification(std::move(write_database_data));

      // Respond with success as this notification got scheduled successfully.
      base::PostTask(FROM_HERE, {BrowserThread::UI},
                     base::BindOnce(std::move(callback), /* success= */ true,
                                    notification_id));
      return;
    }

    // Display the notification immediately.
    write_database_data.notification_resources =
        database_data.notification_resources;
    service_proxy_->DisplayNotification(std::move(write_database_data),
                                        std::move(callback));
    return;
  }

  // Blow away the database if writing data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), /* success= */ false,
                                /* notification_id= */ ""));
}

void PlatformNotificationContextImpl::DeleteNotificationData(
    const std::string& notification_id,
    const GURL& origin,
    bool close_notification,
    DeleteResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!service_proxy_)
    return;

  // Close notification as we're about to delete its data.
  if (close_notification)
    service_proxy_->CloseNotification(notification_id);

  bool should_log_close = service_proxy_->ShouldLogClose(origin);
  LazyInitialize(base::BindOnce(
      &PlatformNotificationContextImpl::DoDeleteNotificationData, this,
      notification_id, origin, std::move(callback), should_log_close));
}

void PlatformNotificationContextImpl::DoDeleteNotificationData(
    const std::string& notification_id,
    const GURL& origin,
    DeleteResultCallback callback,
    bool should_log_close,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), false));
    return;
  }

  // Read additional data if we need to log the close event.
  if (should_log_close) {
    NotificationDatabaseData data;
    if (database_->ReadNotificationData(notification_id, origin, &data) ==
        NotificationDatabase::STATUS_OK) {
      service_proxy_->LogClose(std::move(data));
    }
  }

  NotificationDatabase::Status status =
      database_->DeleteNotificationData(notification_id, origin);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.DeleteResult", status,
                            NotificationDatabase::STATUS_COUNT);

  bool success = status == NotificationDatabase::STATUS_OK;

  // Blow away the database if deleting data failed due to corruption. Following
  // the contract of the delete methods, consider this to be a success as the
  // caller's goal has been achieved: the data is gone.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED) {
    DestroyDatabase();
    success = true;
  }

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), success));
}

void PlatformNotificationContextImpl::OnRegistrationDeleted(
    int64_t registration_id,
    const GURL& pattern) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize(
      base::BindOnce(&PlatformNotificationContextImpl::
                         DoDeleteNotificationsForServiceWorkerRegistration,
                     this, pattern.GetOrigin(), registration_id));
}

void PlatformNotificationContextImpl::
    DoDeleteNotificationsForServiceWorkerRegistration(
        const GURL& origin,
        int64_t service_worker_registration_id,
        bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized)
    return;

  std::set<std::string> deleted_notification_ids;
  NotificationDatabase::Status status =
      database_->DeleteAllNotificationDataForServiceWorkerRegistration(
          origin, service_worker_registration_id, &deleted_notification_ids);

  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.Database.DeleteServiceWorkerRegistrationResult", status,
      NotificationDatabase::STATUS_COUNT);

  // Blow away the database if a corruption error occurred during the deletion.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  if (service_proxy_) {
    for (const std::string& notification_id : deleted_notification_ids)
      service_proxy_->CloseNotification(notification_id);
  }
}

void PlatformNotificationContextImpl::OnStorageWiped() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize(base::BindOnce(
      &PlatformNotificationContextImpl::OnStorageWipedInitialized, this));
}

void PlatformNotificationContextImpl::OnStorageWipedInitialized(
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized)
    return;
  DestroyDatabase();
}

void PlatformNotificationContextImpl::LazyInitialize(
    InitializeResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!task_runner_) {
    task_runner_ =
        base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                         base::TaskPriority::USER_VISIBLE});
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PlatformNotificationContextImpl::OpenDatabase,
                                this, std::move(callback)));
}

void PlatformNotificationContextImpl::OpenDatabase(
    InitializeResultCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (database_) {
    std::move(callback).Run(/* initialized= */ true);
    return;
  }

  database_.reset(new NotificationDatabase(GetDatabasePath(), ukm_callback_));
  NotificationDatabase::Status status =
      database_->Open(/* create_if_missing= */ true);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.OpenResult", status,
                            NotificationDatabase::STATUS_COUNT);

  // When the database could not be opened due to corruption, destroy it, blow
  // away the contents of the directory and try re-opening the database.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED) {
    if (DestroyDatabase()) {
      database_.reset(
          new NotificationDatabase(GetDatabasePath(), ukm_callback_));
      status = database_->Open(/* create_if_missing= */ true);

      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Database.OpenAfterCorruptionResult", status,
          NotificationDatabase::STATUS_COUNT);
    }
  }

  if (status == NotificationDatabase::STATUS_OK) {
    std::move(callback).Run(/* initialized= */ true);
    return;
  }

  database_.reset();

  std::move(callback).Run(/* initialized= */ false);
}

bool PlatformNotificationContextImpl::DestroyDatabase() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(database_);

  NotificationDatabase::Status status = database_->Destroy();
  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.DestroyResult", status,
                            NotificationDatabase::STATUS_COUNT);

  database_.reset();

  // TODO(peter): Close any existing persistent notifications on the platform.

  // Remove all files in the directory that the database was previously located
  // in, to make sure that any left-over files are gone as well.
  base::FilePath database_path = GetDatabasePath();
  if (!database_path.empty())
    return base::DeleteFile(database_path, true);

  return true;
}

base::FilePath PlatformNotificationContextImpl::GetDatabasePath() const {
  if (path_.empty())
    return path_;

  return path_.Append(kPlatformNotificationsDirectory);
}

void PlatformNotificationContextImpl::SetTaskRunnerForTesting(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  task_runner_ = task_runner;
}

}  // namespace content
