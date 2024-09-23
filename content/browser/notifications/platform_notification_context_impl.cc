// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/platform_notification_context_impl.h"

#include <set>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "content/browser/notifications/blink_notification_service_impl.h"
#include "content/browser/notifications/notification_database.h"
#include "content/browser/notifications/notification_trigger_constants.h"
#include "content/browser/notifications/platform_notification_service_proxy.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/platform_notification_service.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {
namespace {

// Name of the directory in the user's profile directory where the notification
// database files should be stored.
const base::FilePath::CharType kPlatformNotificationsDirectory[] =
    FILE_PATH_LITERAL("Platform Notifications");

// Max age of a displayed notification before we consider it stale and remove it
// from the database and ask the platform to close it.
constexpr base::TimeDelta kMaxDisplayedNotificationAge = base::Days(7);

// Checks if this notification can trigger in the future.
bool CanTrigger(const NotificationDatabaseData& data) {
  if (!base::FeatureList::IsEnabled(features::kNotificationTriggers))
    return false;
  return data.notification_data.show_trigger_timestamp && !data.has_triggered;
}

void RecordOldestNotificationTimeUMA(base::Time oldest_notification_time) {
  base::TimeDelta delta = base::Time::Now() - oldest_notification_time;

  base::UmaHistogramCustomCounts(
      "Notifications.Database.OldestNotificationTimeInMinutes",
      delta.InMinutes(), 0, base::Days(150).InMinutes(), 50);
}

// Returns if the notification described by |data| is currently visible.
bool IsVisibleNotification(base::Time start_time,
                           const std::set<std::string>& displayed_notifications,
                           bool supports_synchronization,
                           const NotificationDatabaseData& data) {
  // If a notification can be triggered it has not been shown yet.
  if (CanTrigger(data))
    return false;

  // We can't rely on |displayed_notifications| if the platform does not support
  // synchronization or the notification got added after we got the list of
  // visible notifications. In that case just assume it is visible.
  if (!supports_synchronization || data.creation_time_millis > start_time)
    return true;

  return displayed_notifications.count(data.notification_id);
}

// Checks if the notification described by |data| is currently visible and
// increments |count| by one if so. Then it checks if the creation date of
// the notification is older than the current oldest one. If that is the
// case, |oldest_notification_time| is updated with the date of this
// notification.
void CountVisibleNotifications(
    base::Time start_time,
    const std::set<std::string>& displayed_notifications,
    bool supports_synchronization,
    int* count,
    base::Time* oldest_notification_time,
    const NotificationDatabaseData& data) {
  if (IsVisibleNotification(start_time, displayed_notifications,
                            supports_synchronization, data)) {
    *count = *count + 1;
  }
  if (oldest_notification_time->is_null() ||
      data.creation_time_millis <= *oldest_notification_time) {
    *oldest_notification_time = data.creation_time_millis;
  }
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
      browser_context_->GetPlatformNotificationService();
  if (!service) {
    std::set<std::string> displayed_notifications;
    DidGetNotifications(std::move(displayed_notifications),
                        /* supports_synchronization= */ false);
    return;
  }

  ukm_callback_ = base::BindRepeating(
      &PlatformNotificationServiceProxy::RecordNotificationUkmEvent,
      service_proxy_->AsWeakPtr());

  service->GetDisplayedNotifications(base::BindOnce(
      &PlatformNotificationContextImpl::DidGetNotifications, this));
}

void PlatformNotificationContextImpl::DidGetNotifications(
    std::set<std::string> displayed_notifications,
    bool supports_synchronization) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Abort if the profile has been shut down already. This mainly happens in
  // tests and very short lived sessions.
  if (has_shutdown_.load(std::memory_order_relaxed)) {
    return;
  }

  // Synchronize the notifications stored in the database with the set of
  // displaying notifications in |displayed_notifications|. This is necessary
  // because flakiness may cause a platform to inform Chrome of a notification
  // that has since been closed, or because the platform does not support
  // notifications that exceed the lifetime of the browser process. We can use
  // |lazy| here as there's no point in synchronizing notifications if there is
  // no database to clean them up from.
  InitializeDatabase(
      base::BindOnce(&PlatformNotificationContextImpl::DoSyncNotificationData,
                     this, supports_synchronization,
                     std::move(displayed_notifications)),
      /* lazy= */ true);

  // |service_worker_context_| may be NULL in tests.
  if (service_worker_context_)
    service_worker_context_->AddObserver(this);
}

void PlatformNotificationContextImpl::DoSyncNotificationData(
    bool supports_synchronization,
    std::set<std::string> displayed_notifications,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized) {
    return;
  }

  // Reset |next_trigger_| to keep track of the next trigger timestamp.
  next_trigger_ = std::nullopt;

  // Iterate over all notifications and delete all expired ones.
  std::set<std::string> close_notification_ids;
  NotificationDatabase::Status status =
      database_->ForEachNotificationData(base::BindRepeating(
          &PlatformNotificationContextImpl::DoHandleSyncNotification, this,
          supports_synchronization, displayed_notifications,
          &close_notification_ids));

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  base::UmaHistogramCounts10000(
      "Notifications.Database.ExpiredNotificationCount",
      close_notification_ids.size());

  if (!has_shutdown_.load(std::memory_order_relaxed)) {
    // Schedule the next trigger timestamp.
    if (next_trigger_) {
      GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
          ->PostTask(
              FROM_HERE,
              base::BindOnce(&PlatformNotificationContextImpl::ScheduleTrigger,
                             this, next_trigger_.value()));
    }

    // Close old notifications.
    if (!close_notification_ids.empty()) {
      GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
          ->PostTask(FROM_HERE,
                     base::BindOnce(
                         &PlatformNotificationContextImpl::CloseNotifications,
                         this, close_notification_ids));
    }
  }
}

void PlatformNotificationContextImpl::DoHandleSyncNotification(
    bool supports_synchronization,
    const std::set<std::string>& displayed_notifications,
    std::set<std::string>* close_notification_ids,
    const NotificationDatabaseData& data) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(close_notification_ids);

  // Handle pending notifications.
  if (CanTrigger(data)) {
    base::Time timestamp =
        data.notification_data.show_trigger_timestamp.value();
    // Check if we should display this notification.
    if (timestamp <= base::Time::Now()) {
      DoTriggerNotification(data);
    } else if (!next_trigger_ || next_trigger_.value() > timestamp) {
      next_trigger_ = timestamp;
    }
    return;
  }

  // Delete very old notifications as they are most probably not on screen
  // anymore and their relevance is questionable anyway. We still want to tell
  // the platform to remove them for cleanup just in case.
  base::Time display_time =
      data.notification_data.show_trigger_timestamp.value_or(
          data.creation_time_millis);
  base::TimeDelta age = base::Time::Now() - display_time;
  if (age >= kMaxDisplayedNotificationAge) {
    database_->DeleteNotificationData(data.notification_id, data.origin);
    close_notification_ids->insert(data.notification_id);
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
  has_shutdown_.store(true, std::memory_order_relaxed);

  service_proxy_.reset();

  services_.clear();

  // |service_worker_context_| may be NULL in tests.
  if (service_worker_context_)
    service_worker_context_->RemoveObserver(this);

  browser_context_ = nullptr;
}

void PlatformNotificationContextImpl::CreateService(
    RenderProcessHost* render_process_host,
    const blink::StorageKey& storage_key,
    const GURL& document_url,
    const WeakDocumentPtr& weak_document_ptr,
    RenderProcessHost::NotificationServiceCreatorType creator_type,
    mojo::PendingReceiver<blink::mojom::NotificationService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  services_.push_back(std::make_unique<BlinkNotificationServiceImpl>(
      this, browser_context_, service_worker_context_, render_process_host,
      storage_key, document_url, weak_document_ptr, creator_type,
      std::move(receiver)));
}

void PlatformNotificationContextImpl::RemoveService(
    BlinkNotificationServiceImpl* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::erase_if(
      services_,
      [service](const std::unique_ptr<BlinkNotificationServiceImpl>& ptr) {
        return ptr.get() == service;
      });
}

void PlatformNotificationContextImpl::
    DeleteAllNotificationDataForBlockedOrigins(
        DeleteAllResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  InitializeDatabase(base::BindOnce(
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
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /* success= */ false,
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

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), success, std::move(origins)));
}

void PlatformNotificationContextImpl::CheckPermissionsAndDeleteBlocked(
    DeleteAllResultCallback callback,
    bool success,
    std::set<GURL> origins) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Make sure |browser_context_| is still valid before getting the controller.
  if (!success || has_shutdown_.load(std::memory_order_relaxed)) {
    std::move(callback).Run(/* success= */ false, /* deleted_count= */ 0);
    return;
  }

  content::PermissionController* controller =
      browser_context_->GetPermissionController();
  if (!controller) {
    std::move(callback).Run(/* success= */ false, /* deleted_count= */ 0);
    return;
  }

  // Erase all valid origins so we're left with invalid ones.
  std::erase_if(origins, [controller](const GURL& origin) {
    auto permission = controller
                          ->GetPermissionResultForOriginWithoutContext(
                              blink::PermissionType::NOTIFICATIONS,
                              url::Origin::Create(origin))
                          .status;
    return permission == blink::mojom::PermissionStatus::GRANTED;
  });

  if (origins.empty()) {
    std::move(callback).Run(/* success= */ true, /* deleted_count= */ 0);
    return;
  }

  InitializeDatabase(base::BindOnce(
      &PlatformNotificationContextImpl::DoDeleteAllNotificationDataForOrigins,
      this, std::move(origins), /* tag= */ std::string(),
      /* is_shown_by_browser= */ std::nullopt, std::move(callback)));
}

void PlatformNotificationContextImpl::DoDeleteAllNotificationDataForOrigins(
    std::set<GURL> origins,
    const std::string& tag,
    std::optional<bool> is_shown_by_browser,
    DeleteAllResultCallback callback,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /* success= */ false,
                                  /* deleted_count= */ 0));
    return;
  }

  std::set<std::string> deleted_notification_ids;
  NotificationDatabase::Status status = NotificationDatabase::STATUS_OK;
  for (const auto& origin : origins) {
    status = database_->DeleteAllNotificationDataForOrigin(
        origin, tag, is_shown_by_browser, &deleted_notification_ids);
    if (status != NotificationDatabase::STATUS_OK)
      break;
  }

  bool success = status == NotificationDatabase::STATUS_OK;

  // Blow away the database if deleting data failed due to corruption. Following
  // the contract of the delete methods, consider this to be a success as the
  // caller's goal has been achieved: the data is gone.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED) {
    DestroyDatabase();
    success = true;
  }

  if (!deleted_notification_ids.empty()) {
    GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&PlatformNotificationContextImpl::CloseNotifications,
                           this, deleted_notification_ids));
  }

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success,
                                deleted_notification_ids.size()));
}

void PlatformNotificationContextImpl::DeleteAllNotificationDataWithTag(
    const std::string& tag,
    std::optional<bool> is_shown_by_browser,
    const GURL& origin,
    DeleteAllResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::set<GURL> origins = {origin};
  InitializeDatabase(base::BindOnce(
      &PlatformNotificationContextImpl::DoDeleteAllNotificationDataForOrigins,
      this, std::move(origins), tag, is_shown_by_browser, std::move(callback)));
}

void PlatformNotificationContextImpl::ReadNotificationDataAndRecordInteraction(
    const std::string& notification_id,
    const GURL& origin,
    const PlatformNotificationContext::Interaction interaction,
    ReadResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  InitializeDatabase(base::BindOnce(
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
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /* success= */ false,
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
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /* success= */ true,
                                  database_data));
    return;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /* success= */ false,
                                NotificationDatabaseData()));
}

void PlatformNotificationContextImpl::TriggerNotifications() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::set<std::string> displayed_notifications;
  InitializeDatabase(base::BindOnce(
      &PlatformNotificationContextImpl::DoSyncNotificationData, this,
      /* supports_synchronization= */ false,
      std::move(displayed_notifications)));
}

void PlatformNotificationContextImpl::DoTriggerNotification(
    const NotificationDatabaseData& database_data) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // Bail out in case we can not display the notification after Shutdown.
  if (has_shutdown_.load(std::memory_order_relaxed)) {
    return;
  }

  blink::NotificationResources resources;
  NotificationDatabase::Status status = database_->ReadNotificationResources(
      database_data.notification_id, database_data.origin, &resources);

  if (status != NotificationDatabase::STATUS_OK)
    resources = blink::NotificationResources();

  // Create a copy of the |database_data| to store the |has_triggered| flag.
  NotificationDatabaseData write_database_data = database_data;
  write_database_data.has_triggered = true;
  status = database_->WriteNotificationData(write_database_data.origin,
                                            write_database_data);

  if (status != NotificationDatabase::STATUS_OK) {
    database_->DeleteNotificationData(write_database_data.notification_id,
                                      write_database_data.origin);
    return;
  }

  // Remove resources from DB as we don't need them anymore.
  database_->DeleteNotificationResources(write_database_data.notification_id,
                                         write_database_data.origin);

  write_database_data.notification_resources = std::move(resources);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PlatformNotificationContextImpl::DisplayNotification,
                     this, write_database_data, base::DoNothing()));
}

void PlatformNotificationContextImpl::WriteNotificationResources(
    std::vector<NotificationResourceData> resource_data,
    WriteResourcesResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (has_shutdown_.load(std::memory_order_relaxed)) {
    return;
  }

  InitializeDatabase(base::BindOnce(
      &PlatformNotificationContextImpl::DoWriteNotificationResources, this,
      std::move(resource_data), std::move(callback)));
}

void PlatformNotificationContextImpl::DoWriteNotificationResources(
    std::vector<NotificationResourceData> resource_data,
    WriteResourcesResultCallback callback,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /* success= */ false));
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
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /* success= */ true));
    return;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /* success= */ false));
}

void PlatformNotificationContextImpl::ReDisplayNotifications(
    std::vector<GURL> origins,
    ReDisplayNotificationsResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (has_shutdown_.load(std::memory_order_relaxed)) {
    return;
  }

  InitializeDatabase(
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
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), display_count));
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
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&PlatformNotificationContextImpl::DisplayNotification,
                         this, display_data, base::DoNothing()));
      ++display_count;
    }
    if (status != NotificationDatabase::STATUS_OK)
      break;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
      ->PostTask(FROM_HERE, base::BindOnce(std::move(callback), display_count));
}

void PlatformNotificationContextImpl::ReadNotificationResources(
    const std::string& notification_id,
    const GURL& origin,
    ReadResourcesResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  InitializeDatabase(base::BindOnce(
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
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /* success= */ false,
                                  blink::NotificationResources()));
    return;
  }

  blink::NotificationResources notification_resources;
  NotificationDatabase::Status status = database_->ReadNotificationResources(
      notification_id, origin, &notification_resources);

  if (status == NotificationDatabase::STATUS_OK) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /* success= */ true,
                                  notification_resources));
    return;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /* success= */ false,
                                blink::NotificationResources()));
}

void PlatformNotificationContextImpl::OnGetDisplayedNotifications(
    InitializeGetDisplayedCallback callback,
    std::set<std::string> notification_ids,
    bool supports_synchronization) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  InitializeDatabase(base::BindOnce(std::move(callback),
                                    std::move(notification_ids),
                                    supports_synchronization));
}

void PlatformNotificationContextImpl::TryGetDisplayedNotifications(
    const GURL& origin,
    InitializeGetDisplayedCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PlatformNotificationService* service =
      browser_context_->GetPlatformNotificationService();

  if (!service) {
    // Rely on the database only
    std::set<std::string> notification_ids;
    OnGetDisplayedNotifications(std::move(callback),
                                std::move(notification_ids),
                                /* supports_synchronization= */ false);
    return;
  }

  service->GetDisplayedNotificationsForOrigin(
      origin, base::BindOnce(
                  &PlatformNotificationContextImpl::OnGetDisplayedNotifications,
                  this, std::move(callback)));
}

void PlatformNotificationContextImpl::
    ReadAllNotificationDataForServiceWorkerRegistration(
        const GURL& origin,
        int64_t service_worker_registration_id,
        ReadAllResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  TryGetDisplayedNotifications(
      origin,
      base::BindOnce(&PlatformNotificationContextImpl::
                         DoReadAllNotificationDataForServiceWorkerRegistration,
                     this, base::Time::Now(), origin,
                     service_worker_registration_id, std::move(callback)));
}

void PlatformNotificationContextImpl::
    CountVisibleNotificationsForServiceWorkerRegistration(
        const GURL& origin,
        int64_t service_worker_registration_id,
        CountResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  TryGetDisplayedNotifications(
      origin, base::BindOnce(
                  &PlatformNotificationContextImpl::
                      DoCountVisibleNotificationsForServiceWorkerRegistration,
                  this, base::Time::Now(), origin,
                  service_worker_registration_id, std::move(callback)));
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
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /* success= */ false,
                                  std::vector<NotificationDatabaseData>()));
    return;
  }

  std::vector<NotificationDatabaseData> notification_datas;

  // TODO(crbug.com/40179016): Pass in via an argument whether we want to
  // include notifications shown by the browser or not.
  NotificationDatabase::Status status =
      database_->ReadAllNotificationDataForServiceWorkerRegistration(
          origin, service_worker_registration_id,
          /* is_shown_by_browser= */ false, &notification_datas);

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

    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /* success= */ true,
                                  notification_datas));

    // Remove notifications that are not actually on display anymore.
    for (const auto& it : obsolete_notifications)
      database_->DeleteNotificationData(it, origin);
    return;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /* success= */ false,
                                std::vector<NotificationDatabaseData>()));
}

void PlatformNotificationContextImpl::
    DoCountVisibleNotificationsForServiceWorkerRegistration(
        base::Time start_time,
        const GURL& origin,
        int64_t service_worker_registration_id,
        CountResultCallback callback,
        std::set<std::string> displayed_notifications,
        bool supports_synchronization,
        bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /* success= */ false,
                                  /* count= */ 0));
    return;
  }

  int notification_count = 0;
  base::Time oldest_notification_time;
  NotificationDatabase::Status status =
      database_->ForEachNotificationDataForServiceWorkerRegistration(
          origin, service_worker_registration_id,
          base::BindRepeating(&CountVisibleNotifications, start_time,
                              displayed_notifications, supports_synchronization,
                              &notification_count, &oldest_notification_time));

  if (!oldest_notification_time.is_null())
    RecordOldestNotificationTimeUMA(oldest_notification_time);

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                status == NotificationDatabase::STATUS_OK,
                                notification_count));
}

void PlatformNotificationContextImpl::WriteNotificationData(
    int64_t persistent_notification_id,
    int64_t service_worker_registration_id,
    const GURL& origin,
    const NotificationDatabaseData& database_data,
    WriteResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  InitializeDatabase(base::BindOnce(
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
  if (!initialized || has_shutdown_.load(std::memory_order_relaxed)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /* success= */ false,
                                  /* notification_id= */ ""));
    return;
  }

  bool replaces_existing = false;
  std::string notification_id =
      notification_id_generator_.GenerateForPersistentNotification(
          origin, database_data.notification_data.tag,
          database_data.is_shown_by_browser, persistent_notification_id);

  // Eagerly delete data for replaced notifications from the database.
  if (!database_data.notification_data.tag.empty()) {
    std::set<std::string> deleted_notification_ids;
    NotificationDatabase::Status delete_status =
        database_->DeleteAllNotificationDataForOrigin(
            origin, database_data.notification_data.tag,
            database_data.is_shown_by_browser, &deleted_notification_ids);

    replaces_existing = deleted_notification_ids.count(notification_id) != 0;

    UMA_HISTOGRAM_ENUMERATION("Notifications.Database.DeleteBeforeWriteResult",
                              delete_status,
                              NotificationDatabase::STATUS_COUNT);

    // Unless the database was corrupted following this change, there is no
    // reason to bail out here in event of failure because the notification
    // display logic will handle notification replacement for the user.
    if (delete_status == NotificationDatabase::STATUS_ERROR_CORRUPTED) {
      DestroyDatabase();

      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), /* success= */ false,
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
    // TODO(crbug.com/40596304): Reply with a custom error so developers can
    // handle this.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /* success= */ false,
                                  /* notification_id= */ ""));
    return;
  }

  // Only store resources for notifications that will be scheduled.
  if (!CanTrigger(write_database_data))
    write_database_data.notification_resources = std::nullopt;

  NotificationDatabase::Status status =
      database_->WriteNotificationData(origin, write_database_data);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.WriteResult", status,
                            NotificationDatabase::STATUS_COUNT);

  if (status == NotificationDatabase::STATUS_OK) {
    if (CanTrigger(write_database_data)) {
      if (replaces_existing) {
        std::set<std::string> notification_ids = {notification_id};
        GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
            ->PostTask(FROM_HERE,
                       base::BindOnce(
                           &PlatformNotificationContextImpl::CloseNotifications,
                           this, notification_ids));
      }

      // Schedule notification to be shown.
      GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
          ->PostTask(FROM_HERE,
                     base::BindOnce(
                         &PlatformNotificationContextImpl::ScheduleNotification,
                         this, write_database_data));

      // Respond with success as this notification got scheduled successfully.
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), /* success= */ true,
                                    notification_id));
      return;
    }

    write_database_data.notification_resources =
        database_data.notification_resources;
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&PlatformNotificationContextImpl::DisplayNotification,
                       this, write_database_data, std::move(callback)));
    return;
  }

  // Blow away the database if writing data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /* success= */ false,
                                /* notification_id= */ ""));
}

void PlatformNotificationContextImpl::DeleteNotificationData(
    const std::string& notification_id,
    const GURL& origin,
    bool close_notification,
    DeleteResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (has_shutdown_.load(std::memory_order_relaxed)) {
    return;
  }

  // Close notification as we're about to delete its data.
  if (close_notification) {
    std::set<std::string> notification_ids = {notification_id};
    GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&PlatformNotificationContextImpl::CloseNotifications,
                           this, notification_ids));
  }

  bool should_log_close = service_proxy_->ShouldLogClose(origin);
  InitializeDatabase(base::BindOnce(
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
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  // Read additional data if we need to log the close event.
  if (should_log_close) {
    NotificationDatabaseData data;
    if (database_->ReadNotificationData(notification_id, origin, &data) ==
        NotificationDatabase::STATUS_OK) {
      GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
          ->PostTask(FROM_HERE,
                     base::BindOnce(&PlatformNotificationContextImpl::LogClose,
                                    this, data));
    }
  }

  // TODO(crbug.com/40179016): Should we verify that websites don't try to close
  // notifications shown by the browser (is_shown_by_browser == true)?

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

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success));
}

void PlatformNotificationContextImpl::OnRegistrationDeleted(
    int64_t registration_id,
    const GURL& pattern,
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  InitializeDatabase(base::BindOnce(
      &PlatformNotificationContextImpl::
          DoDeleteNotificationsForServiceWorkerRegistration,
      this, pattern.DeprecatedGetOriginAsURL(), registration_id));
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

  // Blow away the database if a corruption error occurred during the deletion.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  if (!deleted_notification_ids.empty()) {
    GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&PlatformNotificationContextImpl::CloseNotifications,
                           this, deleted_notification_ids));
  }
}

void PlatformNotificationContextImpl::OnStorageWiped() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  InitializeDatabase(base::BindOnce(
      &PlatformNotificationContextImpl::OnStorageWipedInitialized, this));
}

void PlatformNotificationContextImpl::OnStorageWipedInitialized(
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized)
    return;
  DestroyDatabase();
}

void PlatformNotificationContextImpl::InitializeDatabase(
    InitializeResultCallback callback,
    bool lazy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!task_runner_) {
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PlatformNotificationContextImpl::OpenDatabase, this,
                     std::move(callback), /* create_if_missing= */ !lazy));
}

void PlatformNotificationContextImpl::OpenDatabase(
    InitializeResultCallback callback,
    bool create_if_missing) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (database_) {
    std::move(callback).Run(/* initialized= */ true);
    return;
  }

  auto database =
      std::make_unique<NotificationDatabase>(GetDatabasePath(), ukm_callback_);
  NotificationDatabase::Status status = database->Open(create_if_missing);

  // Bail if we don't want to create a new database and there isn't one already.
  if (!create_if_missing &&
      status == NotificationDatabase::STATUS_ERROR_NOT_FOUND) {
    std::move(callback).Run(/* initialized= */ false);
    return;
  }

  // Remember the database so we don't need to open it again.
  database_ = std::move(database);
  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.OpenResult", status,
                            NotificationDatabase::STATUS_COUNT);

  // When the database could not be opened due to corruption, destroy it, blow
  // away the contents of the directory and try re-opening the database.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED) {
    // Bail if we couldn't destroy the corrupted database or if we don't need
    // to create a new one.
    if (!DestroyDatabase() || !create_if_missing) {
      std::move(callback).Run(/* initialized= */ false);
      return;
    }

    // We've destroyed the corrupted database, now try to create a new one.
    database_ = std::make_unique<NotificationDatabase>(GetDatabasePath(),
                                                       ukm_callback_);
    status = database_->Open(create_if_missing);

    UMA_HISTOGRAM_ENUMERATION(
        "Notifications.Database.OpenAfterCorruptionResult", status,
        NotificationDatabase::STATUS_COUNT);
  }

  // Failed to open a valid database. Clear it and try again next time.
  if (status != NotificationDatabase::STATUS_OK) {
    database_.reset();
    std::move(callback).Run(/* initialized= */ false);
    return;
  }

  // All good!
  std::move(callback).Run(/* initialized= */ true);
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
  return database_path.empty() || base::DeletePathRecursively(database_path);
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

void PlatformNotificationContextImpl::DisplayNotification(
    const NotificationDatabaseData& data,
    WriteResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (service_proxy_) {
    service_proxy_->DisplayNotification(data, std::move(callback));
  }
}

void PlatformNotificationContextImpl::CloseNotifications(
    const std::set<std::string>& notification_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (service_proxy_) {
    service_proxy_->CloseNotifications(notification_ids);
  }
}

void PlatformNotificationContextImpl::ScheduleTrigger(base::Time timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (service_proxy_) {
    service_proxy_->ScheduleTrigger(timestamp);
  }
}

void PlatformNotificationContextImpl::ScheduleNotification(
    const NotificationDatabaseData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (service_proxy_) {
    service_proxy_->ScheduleNotification(data);
  }
}

void PlatformNotificationContextImpl::LogClose(
    const NotificationDatabaseData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (service_proxy_) {
    service_proxy_->LogClose(data);
  }
}

}  // namespace content
