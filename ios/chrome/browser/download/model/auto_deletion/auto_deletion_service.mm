// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/auto_deletion/auto_deletion_service.h"

#import "base/apple/foundation_util.h"
#import "base/base64.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
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

// Returns a base::FilePath object pointing to the location on the device where
// `file` persists.
base::FilePath GetFilePathForScheduledFile(
    const auto_deletion::ScheduledFile& file) {
  NSString* filename =
      base::apple::FilePathToNSString(file.filepath().BaseName());
  NSFileManager* manager = [NSFileManager defaultManager];
  NSURL* documentsDirectory =
      [[manager URLsForDirectory:NSDocumentDirectory
                       inDomains:NSUserDomainMask] firstObject];
  NSURL* URL = [NSURL URLWithString:filename relativeToURL:documentsDirectory];
  return base::apple::NSURLToFilePath(URL.absoluteURL);
}

std::optional<base::File::Info> GetFileInfo(base::FilePath path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return std::nullopt;
  }

  base::File::Info info;
  if (!file.GetInfo(&info)) {
    return std::nullopt;
  }

  return info;
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
    base::FilePath file_path = GetFilePathForScheduledFile(file);

    if (!base::PathExists(file_path)) {
      base::UmaHistogramEnumeration(
          kAutoDeletionServiceFileRemovalFailureHistogram,
          AutoDeletionServiceFileRemovalFailures::kFileDoesNotExist);
      continue;
    }

    std::optional<base::File::Info> info = GetFileInfo(file_path);
    if (!info.has_value()) {
      base::UmaHistogramEnumeration(
          kAutoDeletionServiceFileRemovalFailureHistogram,
          AutoDeletionServiceFileRemovalFailures::kFileReadFailure);
      continue;
    }

    if (file.download_time() != info->last_modified) {
      base::UmaHistogramEnumeration(
          kAutoDeletionServiceFileRemovalFailureHistogram,
          AutoDeletionServiceFileRemovalFailures::
              kLastModifiedTimestampMismatch);
      continue;
    }

    if (!base::DeleteFile(file_path)) {
      base::UmaHistogramEnumeration(
          kAutoDeletionServiceFileRemovalFailureHistogram,
          AutoDeletionServiceFileRemovalFailures::kGenericRemovalError);
      continue;
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

void AutoDeletionService::SetDownloadTask(web::DownloadTask* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignore if there is already a download task being processed.
  if (download_task_) {
    return;
  }

  CHECK(task);
  download_task_ = task;
  download_task_details_.enrollment_status =
      DeletionEnrollmentStatus::kUndecided;
  download_task_observation_.Observe(download_task_);
}

void AutoDeletionService::SetEnrollmentStatus(DeletionEnrollmentStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!download_task_) {
    return;
  }

  download_task_details_.enrollment_status = status;
  MaybeScheduleFileForDeletion();
}

void AutoDeletionService::SetDownloadPath(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!download_task_) {
    return;
  }

  download_task_details_.path = path;
  MaybeScheduleFileForDeletion();
}

int64_t AutoDeletionService::GetDownloadSizeInBytes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!download_task_) {
    return -1;
  }

  return download_task_->GetTotalBytes();
}

void AutoDeletionService::RemoveScheduledFilesReadyForDeletion(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scheduler_.Clear();
}

void AutoDeletionService::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Reset the service's internal state.
  weak_ptr_factory_.InvalidateWeakPtrs();
  download_task_observation_.Reset();
  download_task_ = nullptr;
  download_task_details_ = {};
}

void AutoDeletionService::MaybeScheduleFileForDeletion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!AreAllPreconditionsMet()) {
    return;
  }

  if (download_task_details_.enrollment_status !=
      DeletionEnrollmentStatus::kEnrolled) {
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {
          base::MayBlock(),
          base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
          base::ThreadPolicy::PREFER_BACKGROUND,
      },
      base::BindOnce(&GetFileInfo, download_task_details_.path),
      base::BindOnce(&AutoDeletionService::ScheduleFileForDeletion,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AutoDeletionService::ScheduleFileForDeletion(
    std::optional<base::File::Info> info) {
  if (!info.has_value()) {
    return;
  }

  ScheduledFile file(download_task_details_.path, info->last_modified);
  scheduler_.ScheduleFile(file);
  base::UmaHistogramEnumeration(
      kAutoDeletionServiceActionsHistogram,
      AutoDeletionServiceActions::kFileScheduledForAutoDeletion);
}

void AutoDeletionService::OnFilesDeletedFromDisk(base::Time instant,
                                                 base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scheduler_.RemoveExpiredFiles(instant);
  std::move(closure).Run();
}

bool AutoDeletionService::AreAllPreconditionsMet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!download_task_) {
    return false;
  }

  return download_task_details_.enrollment_status ==
             DeletionEnrollmentStatus::kNotEnrolled ||
         (download_task_details_.enrollment_status ==
              DeletionEnrollmentStatus::kEnrolled &&
          download_task_details_.download_complete &&
          !download_task_details_.path.empty());
}

void AutoDeletionService::OnDownloadUpdated(web::DownloadTask* download_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(download_task, download_task_);

  if (!download_task->IsDone()) {
    return;
  }

  download_task_details_.download_complete = true;
  MaybeScheduleFileForDeletion();
}

void AutoDeletionService::OnDownloadDestroyed(
    web::DownloadTask* download_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Reset();
}

}  // namespace auto_deletion
