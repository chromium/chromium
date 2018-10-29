// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/platform_notification_context_impl.h"

#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "content/browser/notifications/blink_notification_service_impl.h"
#include "content/browser/notifications/notification_database.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/platform_notification_service.h"

namespace content {
namespace {

// Name of the directory in the user's profile directory where the notification
// database files should be stored.
const base::FilePath::CharType kPlatformNotificationsDirectory[] =
    FILE_PATH_LITERAL("Platform Notifications");

}  // namespace

PlatformNotificationContextImpl::PlatformNotificationContextImpl(
    const base::FilePath& path,
    BrowserContext* browser_context,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : path_(path),
      browser_context_(browser_context),
      service_worker_context_(service_worker_context) {
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
  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  if (!service) {
    auto displayed_notifications = std::make_unique<std::set<std::string>>();
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&PlatformNotificationContextImpl::InitializeOnIO, this,
                       std::move(displayed_notifications), false));
    return;
  }

  service->GetDisplayedNotifications(
      browser_context_,
      base::Bind(&PlatformNotificationContextImpl::DidGetNotificationsOnUI,
                 this));

  ukm_callback_ = base::BindRepeating(
      &PlatformNotificationService::RecordNotificationUkmEvent,
      base::Unretained(service), browser_context_);
}

void PlatformNotificationContextImpl::DidGetNotificationsOnUI(
    std::unique_ptr<std::set<std::string>> displayed_notifications,
    bool supports_synchronization) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&PlatformNotificationContextImpl::InitializeOnIO, this,
                     std::move(displayed_notifications),
                     supports_synchronization));
}

void PlatformNotificationContextImpl::InitializeOnIO(
    std::unique_ptr<std::set<std::string>> displayed_notifications,
    bool supports_synchronization) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Synchronize the notifications stored in the database with the set of
  // displaying notifications in |displayed_notifications|. This is necessary
  // because flakiness may cause a platform to inform Chrome of a notification
  // that has since been closed, or because the platform does not support
  // notifications that exceed the lifetime of the browser process.

  // TODO(peter): Synchronizing the actual notifications will be done when the
  // persistent notification ids are stable. For M44 we need to support the
  // case where there may be no notifications after a Chrome restart.

  if (supports_synchronization && displayed_notifications->empty()) {
    prune_database_on_open_ = true;
  }

  // |service_worker_context_| may be NULL in tests.
  if (service_worker_context_)
    service_worker_context_->AddObserver(this);
}

void PlatformNotificationContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  services_.clear();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&PlatformNotificationContextImpl::ShutdownOnIO, this));
}

void PlatformNotificationContextImpl::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // |service_worker_context_| may be NULL in tests.
  if (service_worker_context_)
    service_worker_context_->RemoveObserver(this);
}

void PlatformNotificationContextImpl::CreateService(
    const url::Origin& origin,
    blink::mojom::NotificationServiceRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  services_.push_back(std::make_unique<BlinkNotificationServiceImpl>(
      this, browser_context_, service_worker_context_, origin,
      std::move(request)));
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

void PlatformNotificationContextImpl::ReadNotificationDataAndRecordInteraction(
    const std::string& notification_id,
    const GURL& origin,
    const PlatformNotificationContext::Interaction interaction,
    const ReadResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  LazyInitialize(
      base::Bind(&PlatformNotificationContextImpl::DoReadNotificationData, this,
                 notification_id, origin, interaction, callback),
      base::Bind(callback, false /* success */, NotificationDatabaseData()));
}

void PlatformNotificationContextImpl::DoReadNotificationData(
    const std::string& notification_id,
    const GURL& origin,
    Interaction interaction,
    const ReadResultCallback& callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  NotificationDatabaseData database_data;
  NotificationDatabase::Status status =
      database_->ReadNotificationDataAndRecordInteraction(
          notification_id, origin, interaction, &database_data);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.ReadResult", status,
                            NotificationDatabase::STATUS_COUNT);

  if (status == NotificationDatabase::STATUS_OK) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(callback, true /* success */, database_data));
    return;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(callback, false /* success */,
                                          NotificationDatabaseData()));
}

void PlatformNotificationContextImpl::
    SynchronizeDisplayedNotificationsForServiceWorkerRegistrationOnUI(
        const GURL& origin,
        int64_t service_worker_registration_id,
        const ReadAllResultCallback& callback,
        std::unique_ptr<std::set<std::string>> notification_ids,
        bool supports_synchronization) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &PlatformNotificationContextImpl::
              SynchronizeDisplayedNotificationsForServiceWorkerRegistrationOnIO,
          this, origin, service_worker_registration_id, callback,
          std::move(notification_ids), supports_synchronization));
}

void PlatformNotificationContextImpl::
    SynchronizeDisplayedNotificationsForServiceWorkerRegistrationOnIO(
        const GURL& origin,
        int64_t service_worker_registration_id,
        const ReadAllResultCallback& callback,
        std::unique_ptr<std::set<std::string>> notification_ids,
        bool supports_synchronization) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LazyInitialize(
      base::Bind(&PlatformNotificationContextImpl::
                     DoReadAllNotificationDataForServiceWorkerRegistration,
                 this, origin, service_worker_registration_id, callback,
                 base::Passed(&notification_ids), supports_synchronization),
      base::Bind(callback, false /* success */,
                 std::vector<NotificationDatabaseData>()));
}

void PlatformNotificationContextImpl::
    ReadAllNotificationDataForServiceWorkerRegistration(
        const GURL& origin,
        int64_t service_worker_registration_id,
        const ReadAllResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto notification_ids = std::make_unique<std::set<std::string>>();

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();

  if (!service) {
    // Rely on the database only
    SynchronizeDisplayedNotificationsForServiceWorkerRegistrationOnIO(
        origin, service_worker_registration_id, callback,
        std::move(notification_ids), false /* supports_synchronization */);
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &PlatformNotificationService::GetDisplayedNotifications,
          base::Unretained(service), browser_context_,
          base::Bind(
              &PlatformNotificationContextImpl::
                  SynchronizeDisplayedNotificationsForServiceWorkerRegistrationOnUI,
              this, origin, service_worker_registration_id, callback)));
}

void PlatformNotificationContextImpl::
    DoReadAllNotificationDataForServiceWorkerRegistration(
        const GURL& origin,
        int64_t service_worker_registration_id,
        const ReadAllResultCallback& callback,
        std::unique_ptr<std::set<std::string>> displayed_notifications,
        bool supports_synchronization) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(displayed_notifications);

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
        if (displayed_notifications->count(it->notification_id)) {
          ++it;
        } else {
          obsolete_notifications.push_back(it->notification_id);
          it = notification_datas.erase(it);
        }
      }
    }

    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(callback, true /* success */, notification_datas));

    // Remove notifications that are not actually on display anymore.
    for (const auto& it : obsolete_notifications)
      database_->DeleteNotificationData(it, origin);
    return;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(callback, false /* success */,
                     std::vector<NotificationDatabaseData>()));
}

void PlatformNotificationContextImpl::WriteNotificationData(
    int64_t persistent_notification_id,
    int64_t service_worker_registration_id,
    const GURL& origin,
    const NotificationDatabaseData& database_data,
    const WriteResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LazyInitialize(
      base::Bind(&PlatformNotificationContextImpl::DoWriteNotificationData,
                 this, service_worker_registration_id,
                 persistent_notification_id, origin, database_data, callback),
      base::Bind(callback, false /* success */, "" /* notification_id */));
}

void PlatformNotificationContextImpl::DoWriteNotificationData(
    int64_t service_worker_registration_id,
    int64_t persistent_notification_id,
    const GURL& origin,
    const NotificationDatabaseData& database_data,
    const WriteResultCallback& callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(database_data.notification_id.empty());

  // Eagerly delete data for replaced notifications from the database.
  if (!database_data.notification_data.tag.empty()) {
    std::set<std::string> deleted_notification_ids;
    NotificationDatabase::Status delete_status =
        database_->DeleteAllNotificationDataForOrigin(
            origin, database_data.notification_data.tag,
            &deleted_notification_ids);

    UMA_HISTOGRAM_ENUMERATION("Notifications.Database.DeleteBeforeWriteResult",
                              delete_status,
                              NotificationDatabase::STATUS_COUNT);

    // Unless the database was corrupted following this change, there is no
    // reason to bail out here in event of failure because the notification
    // display logic will handle notification replacement for the user.
    if (delete_status == NotificationDatabase::STATUS_ERROR_CORRUPTED) {
      DestroyDatabase();

      base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                               base::BindOnce(callback, false /* success */,
                                              "" /* notification_id */));
      return;
    }
  }

  // Create a copy of the |database_data| to store a generated notification ID.
  NotificationDatabaseData write_database_data = database_data;
  write_database_data.notification_id =
      notification_id_generator_.GenerateForPersistentNotification(
          origin, database_data.notification_data.tag,
          persistent_notification_id);

  NotificationDatabase::Status status =
      database_->WriteNotificationData(origin, write_database_data);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.WriteResult", status,
                            NotificationDatabase::STATUS_COUNT);

  if (status == NotificationDatabase::STATUS_OK) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(callback, true /* success */,
                       write_database_data.notification_id));

    return;
  }

  // Blow away the database if writing data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(callback, false /* success */, "" /* notification_id */));
}

void PlatformNotificationContextImpl::DeleteNotificationData(
    const std::string& notification_id,
    const GURL& origin,
    const DeleteResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  LazyInitialize(
      base::Bind(&PlatformNotificationContextImpl::DoDeleteNotificationData,
                 this, notification_id, origin, callback),
      base::Bind(callback, false /* success */));
}

void PlatformNotificationContextImpl::DoDeleteNotificationData(
    const std::string& notification_id,
    const GURL& origin,
    const DeleteResultCallback& callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

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

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(callback, success));
}

void PlatformNotificationContextImpl::OnRegistrationDeleted(
    int64_t registration_id,
    const GURL& pattern) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LazyInitialize(
      base::Bind(&PlatformNotificationContextImpl::
                     DoDeleteNotificationsForServiceWorkerRegistration,
                 this, pattern.GetOrigin(), registration_id),
      base::DoNothing());
}

void PlatformNotificationContextImpl::
    DoDeleteNotificationsForServiceWorkerRegistration(
        const GURL& origin,
        int64_t service_worker_registration_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

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

  // TODO(peter): Close the notifications in |deleted_notification_ids|. See
  // https://crbug.com/532436.
}

void PlatformNotificationContextImpl::OnStorageWiped() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LazyInitialize(
      base::Bind(
          base::IgnoreResult(&PlatformNotificationContextImpl::DestroyDatabase),
          this),
      base::DoNothing());
}

void PlatformNotificationContextImpl::LazyInitialize(
    const base::Closure& success_closure,
    const base::Closure& failure_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!task_runner_) {
    task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PlatformNotificationContextImpl::OpenDatabase,
                                this, success_closure, failure_closure));
}

void PlatformNotificationContextImpl::OpenDatabase(
    const base::Closure& success_closure,
    const base::Closure& failure_closure) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (database_) {
    success_closure.Run();
    return;
  }

  database_.reset(new NotificationDatabase(GetDatabasePath(), ukm_callback_));
  NotificationDatabase::Status status =
      database_->Open(true /* create_if_missing */);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.OpenResult", status,
                            NotificationDatabase::STATUS_COUNT);

  // TODO(peter): Do finer-grained synchronization here.
  if (prune_database_on_open_) {
    prune_database_on_open_ = false;
    DestroyDatabase();

    database_.reset(new NotificationDatabase(GetDatabasePath(), ukm_callback_));
    status = database_->Open(true /* create_if_missing */);

    // TODO(peter): Find the appropriate UMA to cover in regards to
    // synchronizing notifications after the implementation is complete.
  }

  // When the database could not be opened due to corruption, destroy it, blow
  // away the contents of the directory and try re-opening the database.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED) {
    if (DestroyDatabase()) {
      database_.reset(
          new NotificationDatabase(GetDatabasePath(), ukm_callback_));
      status = database_->Open(true /* create_if_missing */);

      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Database.OpenAfterCorruptionResult", status,
          NotificationDatabase::STATUS_COUNT);
    }
  }

  if (status == NotificationDatabase::STATUS_OK) {
    success_closure.Run();
    return;
  }

  database_.reset();

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO}, failure_closure);
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
