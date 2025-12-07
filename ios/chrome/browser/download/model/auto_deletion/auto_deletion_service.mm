// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/auto_deletion/auto_deletion_service.h"

#import "base/apple/foundation_util.h"
#import "base/base64.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "base/time/time.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "crypto/hash.h"
#import "ios/chrome/browser/download/model/auto_deletion/auto_deletion_histograms.h"
#import "ios/chrome/browser/download/model/auto_deletion/scheduled_file.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/web/public/download/download_task.h"

namespace {

// Creates a SHA256 hash of the downloaded file's contents. This hash is used to
// verify that the file that is scheduled to be deleted is the same file that
// was originally scheduled for deletion.
std::string HashDownloadData(base::span<const uint8_t> data) {
  return base::HexEncodeLower(crypto::hash::Sha256(data));
}

// Removes the ScheduledFiles from the device. It is intended to be invoked on a
// background thread.
void RemoveScheduledFilesHelper(
    const std::vector<auto_deletion::ScheduledFile>& files_to_delete) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  // Delete the files from the file system.
  for (const auto& file : files_to_delete) {
    base::UmaHistogramEnumeration(
        kAutoDeletionServiceActionsHistogram,
        AutoDeletionServiceActions::kScheduledFileIdentifiedForRemoval);
    NSString* filename =
        base::apple::FilePathToNSString(file.filepath().BaseName());
    NSFileManager* manager = [NSFileManager defaultManager];
    NSURL* documentsDirectory =
        [[manager URLsForDirectory:NSDocumentDirectory
                         inDomains:NSUserDomainMask] firstObject];
    NSURL* URL = [NSURL URLWithString:filename
                        relativeToURL:documentsDirectory];
    NSString* path = URL.absoluteURL.path;

    if (![manager fileExistsAtPath:path]) {
      base::UmaHistogramEnumeration(
          kAutoDeletionServiceFileRemovalFailureHistogram,
          AutoDeletionServiceFileRemovalFailures::kFileDoesNotExist);
      continue;
    }

    // TODO(crbug.com/455800280): Use `base::ReadFileToBytes()` to read the file
    // into `buffer`.
    std::vector<uint8_t> buffer;
    const std::string hash = HashDownloadData(buffer);

    if (hash != file.hash()) {
      base::UmaHistogramEnumeration(
          kAutoDeletionServiceFileRemovalFailureHistogram,
          AutoDeletionServiceFileRemovalFailures::kHashMismatch);
      return;
    }

    NSError* error;
    [manager removeItemAtPath:path error:&error];
    if (error) {
      base::UmaHistogramEnumeration(
          kAutoDeletionServiceFileRemovalFailureHistogram,
          AutoDeletionServiceFileRemovalFailures::kGenericRemovalError);
      return;
    }
    base::UmaHistogramEnumeration(
        kAutoDeletionServiceActionsHistogram,
        AutoDeletionServiceActions::kScheduledFileRemovedFromDevice);
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

void AutoDeletionService::MarkTaskForDeletion(web::DownloadTask* task,
                                              DeletionEnrollmentStatus status) {
  std::string task_id = base::SysNSStringToUTF8(task->GetIdentifier());
  auto waiting_task = tasks_awaiting_scheduling_.find(task_id);
  if (waiting_task == tasks_awaiting_scheduling_.end()) {
    const base::FilePath path;
    DownloadTaskDetails details =
        DownloadTaskDetails::DetailsForEnrollmentDecision(status);
    tasks_awaiting_scheduling_.insert({task_id, details});
  } else {
    waiting_task->second.enrollment_status = status;
  }

  if (status != DeletionEnrollmentStatus::kEnrolled) {
    return;
  }

  // Schedules the task for observation if it is still downloading content.
  if (!task->IsDone()) {
    if (!download_tasks_observation_.IsObservingSource(task)) {
      download_tasks_observation_.AddObservation(task);
    }
    return;
  }

  MaybeRemoveObservation(task);
  task->GetResponseData(
      base::BindOnce(&AutoDeletionService::MarkTaskForDeletionHelper,
                     weak_ptr_factory_.GetWeakPtr(), task_id));
}

void AutoDeletionService::MarkTaskForDeletion(web::DownloadTask* task,
                                              const base::FilePath& path) {
  std::string task_id = base::SysNSStringToUTF8(task->GetIdentifier());
  auto iterator = tasks_awaiting_scheduling_.find(task_id);
  if (iterator == tasks_awaiting_scheduling_.end()) {
    DownloadTaskDetails details =
        DownloadTaskDetails::DetailsForPermanentPath(path);
    tasks_awaiting_scheduling_.insert({task_id, details});
  } else {
    iterator->second.path = path;
  }

  // Schedules the task for observation if it is still downloading content.
  if (!task->IsDone()) {
    if (!download_tasks_observation_.IsObservingSource(task)) {
      download_tasks_observation_.AddObservation(task);
    }
    return;
  }

  MaybeRemoveObservation(task);
  task->GetResponseData(
      base::BindOnce(&AutoDeletionService::MarkTaskForDeletionHelper,
                     weak_ptr_factory_.GetWeakPtr(), task_id));
}

void AutoDeletionService::ScheduleFileForDeletion(const std::string& task_id) {
  if (!AreAllPreconditionsMet(task_id)) {
    return;
  }

  auto iterator = tasks_awaiting_scheduling_.find(task_id);
  DCHECK(iterator != tasks_awaiting_scheduling_.end());
  DownloadTaskDetails details = iterator->second;
  if (details.enrollment_status == DeletionEnrollmentStatus::kEnrolled) {
    ScheduledFile file(
        details.path,
        HashDownloadData(base::apple::NSDataToSpan(details.file_content)),
        base::Time::Now());
    scheduler_.ScheduleFile(file);
    base::UmaHistogramEnumeration(
        kAutoDeletionServiceActionsHistogram,
        AutoDeletionServiceActions::kFileScheduledForAutoDeletion);
  }
  tasks_awaiting_scheduling_.erase(iterator);
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

void AutoDeletionService::MarkTaskForDeletionHelper(const std::string& task_id,
                                                    NSData* data) {
  auto iterator = tasks_awaiting_scheduling_.find(task_id);
  DCHECK(iterator != tasks_awaiting_scheduling_.end());
  iterator->second.file_content = data;

  ScheduleFileForDeletion(task_id);
}

void AutoDeletionService::OnFilesDeletedFromDisk(base::Time instant,
                                                 base::OnceClosure closure) {
  scheduler_.RemoveExpiredFiles(instant);
  std::move(closure).Run();
}

bool AutoDeletionService::AreAllPreconditionsMet(const std::string& task_id) {
  auto iterator = tasks_awaiting_scheduling_.find(task_id);
  DCHECK(iterator != tasks_awaiting_scheduling_.end());
  DownloadTaskDetails details = iterator->second;

  return details.enrollment_status == DeletionEnrollmentStatus::kNotEnrolled ||
         (details.enrollment_status == DeletionEnrollmentStatus::kEnrolled &&
          !details.path.empty());
}

void AutoDeletionService::MaybeRemoveObservation(web::DownloadTask* task) {
  if (download_tasks_observation_.IsObservingSource(task)) {
    download_tasks_observation_.RemoveObservation(task);
  }
}

void AutoDeletionService::OnDownloadUpdated(web::DownloadTask* download_task) {
  if (!download_task->IsDone()) {
    return;
  }
  download_tasks_observation_.RemoveObservation(download_task);
  std::string task_id = base::SysNSStringToUTF8(download_task->GetIdentifier());
  download_task->GetResponseData(
      base::BindOnce(&AutoDeletionService::MarkTaskForDeletionHelper,
                     weak_ptr_factory_.GetWeakPtr(), task_id));
}

void AutoDeletionService::OnDownloadDestroyed(
    web::DownloadTask* download_task) {
  download_tasks_observation_.RemoveObservation(download_task);
}

}  // namespace auto_deletion
