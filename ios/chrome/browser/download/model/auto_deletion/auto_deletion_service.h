// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_AUTO_DELETION_SERVICE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_AUTO_DELETION_SERVICE_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"
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

// Service responsible for the orchestration of the various functionality within
// the auto-deletion system.
class AutoDeletionService : public web::DownloadTaskObserver {
 public:
  explicit AutoDeletionService(PrefService* local_state);
  ~AutoDeletionService() override;

  // Registers the auto deletion Chrome settings status.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Schedules a file for auto-deletion.
  void ScheduleFileForDeletion(web::DownloadTask* task);

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
  void ScheduleFileForDeletionHelper(web::DownloadTask* task, NSData* data);

  // Notifies the Scheduler to remove its expired ScheduledFiles.
  void OnFilesDeletedFromDisk(base::Time instant, base::OnceClosure closure);

  // web::DownloadTaskObserver:
  void OnDownloadUpdated(web::DownloadTask* download_task) override;
  void OnDownloadDestroyed(web::DownloadTask* download_task) override;

  // The Scheduler object which tracks and manages the downloaded files
  // scheduled for automatic deletion.
  Scheduler scheduler_;

  // Weak factory.
  base::WeakPtrFactory<AutoDeletionService> weak_ptr_factory_{this};
};

}  // namespace auto_deletion

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_AUTO_DELETION_SERVICE_H_
