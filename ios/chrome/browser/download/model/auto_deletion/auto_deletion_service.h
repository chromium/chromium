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
#import "base/scoped_multi_source_observation.h"
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

  // Creates a DownloadTaskDetails struct for when the permanent path is known
  // to the AutoDeletionService but before the user's enrollment decision is
  // set.
  static DownloadTaskDetails DetailsForPermanentPath(
      const base::FilePath permanent_path) {
    DownloadTaskDetails details;
    details.enrollment_status = DeletionEnrollmentStatus::kUndecided;
    details.file_content = nil;
    details.path = permanent_path;
    return details;
  }

  // Creates a DownloadTaskDetails struct for when the user's enrollment
  // decision is known to the AutoDeletionService but before the downloaded
  // file's permanent path is set.
  static DownloadTaskDetails DetailsForEnrollmentDecision(
      DeletionEnrollmentStatus decision) {
    DownloadTaskDetails details;
    details.enrollment_status = decision;
    details.file_content = nil;
    return details;
  }
};

// Service responsible for the orchestration of the various functionality within
// the auto-deletion system.
class AutoDeletionService : public web::DownloadTaskObserver {
 public:
  explicit AutoDeletionService(PrefService* local_state);
  ~AutoDeletionService() override;

  // Registers the auto deletion Chrome settings status.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // The file downloaded by `task` is first stored at a temporary location
  // before being moved to a permanent location. The file's final location is
  // needed when scheduling the file for Auto-deletion. However, this
  // information is not known when the user informs the AutoDeletionService of
  // their intent to enroll `task` in Auto-deletion. Thus, this function stores
  // a reference (i.e marks) to `task` for later when `task` is moved to its
  // permanent location and it is scheduled for Auto-deletion if
  // `isScheduledForAutoDeletion` is true.
  void MarkTaskForDeletion(web::DownloadTask* task,
                           DeletionEnrollmentStatus status);

  // The file downloaded by `task` is first stored at a temporary location
  // before being moved to a permanent location. The user's consent to
  // schedule the file for Auto-deletion is needed before it can be done. Thus,
  // this function stores a reference (i.e marks) to `task` and associates its
  // file location `path` for later when the user decides to enable or disable
  // Auto-deletion for `task`.
  void MarkTaskForDeletion(web::DownloadTask* task, const base::FilePath& path);

  // Schedules a file for auto-deletion if the DownloadTask associated with
  // `task_id` was marked for Auto-deletion.
  void ScheduleFileForDeletion(const std::string& task_id);

  // Deletes the files that have been marked as ready for deletion.
  void RemoveScheduledFilesReadyForDeletion(base::OnceClosure closure);

  // Untracks all the files that were scheduled for auto-deletion.
  void Clear();

 private:
  base::ScopedMultiSourceObservation<web::DownloadTask,
                                     web::DownloadTaskObserver>
      download_tasks_observation_{this};

  // Invoked after the download task data is read from data. It finishes
  // scheduling the file for deletion.
  void MarkTaskForDeletionHelper(const std::string& task_id, NSData* data);

  // Notifies the Scheduler to remove its expired ScheduledFiles.
  void OnFilesDeletedFromDisk(base::Time instant, base::OnceClosure closure);

  // Checks whether all the preconditions necessary to schedule a file for
  // Auto-deletion are set.
  bool AreAllPreconditionsMet(const std::string& task);

  // Removes the task from observation once it is finished.
  void MaybeRemoveObservation(web::DownloadTask* task);

  // web::DownloadTaskObserver:
  void OnDownloadUpdated(web::DownloadTask* download_task) override;
  void OnDownloadDestroyed(web::DownloadTask* download_task) override;

  // The Scheduler object which tracks and manages the downloaded files
  // scheduled for automatic deletion.
  Scheduler scheduler_;

  // This map stores DownloadTasks objects that are waiting for either the user
  // to decided whether to enroll them in Auto-deletion or the downloaded file's
  // permanent file location.
  std::unordered_map<std::string, DownloadTaskDetails>
      tasks_awaiting_scheduling_;

  // Weak factory.
  base::WeakPtrFactory<AutoDeletionService> weak_ptr_factory_{this};
};

}  // namespace auto_deletion

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_AUTO_DELETION_SERVICE_H_
