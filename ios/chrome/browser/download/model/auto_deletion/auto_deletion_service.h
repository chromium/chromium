// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_AUTO_DELETION_SERVICE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_AUTO_DELETION_SERVICE_H_

#import <Foundation/Foundation.h>

#import "base/files/file_path.h"
#import "base/functional/callback_forward.h"
#import "base/memory/raw_ref.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/download/model/auto_deletion/scheduler.h"
#import "ios/web/public/download/download_task_observer.h"

namespace base {
class Time;
}  // namespace base
class PrefRegistrySimple;
class PrefService;
namespace web {
class DownloadTask;
}  // namespace web

namespace auto_deletion {
class Scheduler;

// An enum value that stores whether the user's decision on enroll the file
// in Auto-deletion has been made.
enum class DeletionEnrollmentStatus {
  kUndecided,
  kEnrolled,
  kNotEnrolled,
};

// This struct contains the details that the Auto-deletion system needs to
// complete scheduling the file for Auto-deletion.
struct DownloadTaskDetails {
  // The DeletionEnrollmentStatus associated with the web::DownloadTask.
  DeletionEnrollmentStatus enrollment_status;
  // The data contained within DownloadTask
  NSData* file_content;
  // The filepath at which the downloaded content is located.
  base::FilePath path;
};

// Service responsible for the orchestration of the various functionality within
// the auto-deletion system.
class AutoDeletionService : public web::DownloadTaskObserver {
 public:
  explicit AutoDeletionService(PrefService* local_state);
  ~AutoDeletionService() override;

  // Registers the auto deletion Chrome settings status.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Sets the download task that the AutoDeletionService should process. The
  // download task is expected to be set once per download and before the other
  // setters functions are called.
  void SetDownloadTask(web::DownloadTask*);

  // Sets the enrollment status of the download task.
  void SetEnrollmentStatus(DeletionEnrollmentStatus status);

  // Sets the file path where the downloaded content is permanently located.
  void SetDownloadPath(const base::FilePath& path);

  // Returns the download's content size in bytes.
  int64_t GetDownloadSizeInBytes();

  // Deletes the files that have been marked as ready for deletion.
  void RemoveScheduledFilesReadyForDeletion(base::OnceClosure closure);

  // Untracks all the files that were scheduled for auto-deletion.
  void Clear();

  // Resets the AutoDeletionService to prepare for an upcoming download.
  void Reset();

 private:
  base::ScopedObservation<web::DownloadTask, web::DownloadTaskObserver>
      download_task_observation_{this};

  // Invoked after the download task data is read from data. It finishes
  // scheduling the file for deletion.
  void SetFileContent(NSData* data);

  // Schedules the download task for auto-deletion if all preconditions are met.
  void MaybeScheduleFileForDeletion();

  // Notifies the Scheduler to remove its expired ScheduledFiles.
  void OnFilesDeletedFromDisk(base::Time instant, base::OnceClosure closure);

  // Checks whether all the preconditions necessary to schedule a file for
  // Auto-deletion are set.
  bool AreAllPreconditionsMet();

  // web::DownloadTaskObserver:
  void OnDownloadUpdated(web::DownloadTask* download_task) override;
  void OnDownloadDestroyed(web::DownloadTask* download_task) override;

  SEQUENCE_CHECKER(sequence_checker_);

  // The Scheduler object which tracks and manages the downloaded files
  // scheduled for automatic deletion.
  Scheduler scheduler_;

  // The download task that the AutoDeletionService is currently processing.
  raw_ptr<web::DownloadTask> download_task_;

  // The details associated with the download task that the AutoDeletionService
  // is currently processing.
  DownloadTaskDetails download_task_details_;

  // Weak factory.
  base::WeakPtrFactory<AutoDeletionService> weak_ptr_factory_{this};
};

}  // namespace auto_deletion

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_AUTO_DELETION_SERVICE_H_
