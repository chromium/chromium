// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/auto_deletion/auto_deletion_service.h"

#import "base/apple/foundation_util.h"
#import "base/base64.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/hash/md5.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "base/time/time.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/download/model/auto_deletion/scheduled_file.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/web/public/download/download_task.h"

namespace {

// Creates an MD5Hash of the downloaded file's contents. This hash is used to
// verify that the file that is scheduled to be deleted is the same file that
// was originally scheduled for deletion.
std::string HashDownloadData(base::span<const uint8_t> data_span) {
  base::MD5Digest hash;
  base::MD5Sum(data_span, &hash);
  return base::MD5DigestToBase16(hash);
}

// Removes the ScheduledFiles from the device. It is intended to be invoked on a
// background thread.
void RemoveScheduledFilesHelper(
    const std::vector<auto_deletion::ScheduledFile>& files_to_delete) {
  // Delete the files from the file system.
  std::string buffer;
  for (const auto& file : files_to_delete) {
    if (!base::ReadFileToString(file.filepath(), &buffer)) {
      continue;
    }
    const std::string hash = HashDownloadData(base::as_byte_span(buffer));
    if (hash == file.hash()) {
      base::DeleteFile(file.filepath());
    }
  }
}

}  // namespace

namespace auto_deletion {

AutoDeletionService::AutoDeletionService(PrefService* local_state)
    : scheduler_(local_state) {}

AutoDeletionService::~AutoDeletionService() = default;

// static
void AutoDeletionService::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDownloadAutoDeletionEnabled, false);
  registry->RegisterBooleanPref(prefs::kDownloadAutoDeletionIPHShown, false);
  registry->RegisterListPref(prefs::kDownloadAutoDeletionScheduledFiles);
}

void AutoDeletionService::ScheduleFileForDeletion(web::DownloadTask* task) {
  if (!task->IsDone()) {
    if (!download_tasks_observation_.IsObservingSource(task)) {
      download_tasks_observation_.AddObservation(task);
    }
    return;
  }

  if (download_tasks_observation_.IsObservingSource(task)) {
    download_tasks_observation_.RemoveObservation(task);
  }

  task->GetResponseData(
      base::BindOnce(&AutoDeletionService::ScheduleFileForDeletionHelper,
                     weak_ptr_factory_.GetWeakPtr(), std::move(task)));
}

void AutoDeletionService::RemoveScheduledFilesReadyForDeletion(
    base::OnceClosure closure) {
  // Identify all of the files that are ready for deletion.
  const base::Time now = base::Time::Now();
  std::vector<ScheduledFile> files_to_delete =
      scheduler_.IdentifyExpiredFiles(now);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {
          base::MayBlock(),
          base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
          base::ThreadPolicy::PREFER_BACKGROUND,
      },
      base::BindOnce(&RemoveScheduledFilesHelper, files_to_delete),
      base::BindOnce(&AutoDeletionService::OnFilesDeletedFromDisk,
                     weak_ptr_factory_.GetWeakPtr(), now, std::move(closure)));
}

void AutoDeletionService::Clear() {
  scheduler_.Clear();
}

void AutoDeletionService::ScheduleFileForDeletionHelper(web::DownloadTask* task,
                                                        NSData* data) {
  ScheduledFile file(task->GetResponsePath(),
                     HashDownloadData(base::apple::NSDataToSpan(data)),
                     base::Time::Now());
  scheduler_.ScheduleFile(std::move(file));
}

void AutoDeletionService::OnFilesDeletedFromDisk(base::Time instant,
                                                 base::OnceClosure closure) {
  scheduler_.RemoveExpiredFiles(instant);
  std::move(closure).Run();
}

void AutoDeletionService::OnDownloadUpdated(web::DownloadTask* download_task) {
  ScheduleFileForDeletion(download_task);
}

void AutoDeletionService::OnDownloadDestroyed(
    web::DownloadTask* download_task) {
  download_tasks_observation_.RemoveObservation(download_task);
}

}  // namespace auto_deletion
