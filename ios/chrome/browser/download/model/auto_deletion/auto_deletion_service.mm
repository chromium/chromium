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

    std::optional<std::vector<uint8_t>> buffer =
        base::ReadFileToBytes(file_path);
    if (!buffer.has_value()) {
      base::UmaHistogramEnumeration(
          kAutoDeletionServiceFileRemovalFailureHistogram,
          AutoDeletionServiceFileRemovalFailures::kFileReadFailure);
      continue;
    }

    const std::string hash = HashDownloadData(buffer.value());
    if (hash != file.hash()) {
      base::UmaHistogramEnumeration(
          kAutoDeletionServiceFileRemovalFailureHistogram,
          AutoDeletionServiceFileRemovalFailures::kHashMismatch);
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
  download_task_details_.file_content = nullptr;
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

  if (download_task_details_.enrollment_status ==
      DeletionEnrollmentStatus::kEnrolled) {
    ScheduledFile file(download_task_details_.path,
                       HashDownloadData(base::apple::NSDataToSpan(
                           download_task_details_.file_content)),
                       base::Time::Now());
    scheduler_.ScheduleFile(file);
    base::UmaHistogramEnumeration(
        kAutoDeletionServiceActionsHistogram,
        AutoDeletionServiceActions::kFileScheduledForAutoDeletion);
  }
}

void AutoDeletionService::SetFileContent(NSData* data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (data == nullptr) {
    return;
  }

  download_task_details_.file_content = data;
  MaybeScheduleFileForDeletion();
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
          !download_task_details_.path.empty() &&
          download_task_details_.file_content != nullptr);
}

void AutoDeletionService::OnDownloadUpdated(web::DownloadTask* download_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(download_task, download_task_);

  if (!download_task->IsDone()) {
    return;
  }

  download_task->GetResponseData(base::BindOnce(
      &AutoDeletionService::SetFileContent, weak_ptr_factory_.GetWeakPtr()));
}

void AutoDeletionService::OnDownloadDestroyed(
    web::DownloadTask* download_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Reset();
}

}  // namespace auto_deletion
