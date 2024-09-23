// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_TAB_HELPER_H_

#import "base/scoped_observation.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/lazy_web_state_user_data.h"
#import "ios/web/public/web_state_observer.h"

class DriveUploadTask;
@protocol SystemIdentity;
class UploadTask;

// Manages Save to Drive tab-scoped state i.e. if a `DownloadTask` is received
// through AddDownloadToSaveToDrive(...) then this tab helper
// - 1 - memorises the task, what `SystemIdentity` will be used to save the
// downloaded file to Drive, etc.
// - 2 - observes the `DownloadTask` and uploads the downloaded file using the
// Drive service upon completion of the task.
class DriveTabHelper : public web::LazyWebStateUserData<DriveTabHelper>,
                       public web::DownloadTaskObserver {
 public:
  DriveTabHelper(const DriveTabHelper&) = delete;
  DriveTabHelper& operator=(const DriveTabHelper&) = delete;
  ~DriveTabHelper() override;

  // Adds a DownloadTask to Save to Drive. This download task will be observed
  // and the downloaded file will be uploaded to Drive.
  void AddDownloadToSaveToDrive(web::DownloadTask* task,
                                id<SystemIdentity> identity);
  // Returns the upload task associated with `download_task`, if any.
  UploadTask* GetUploadTaskForDownload(web::DownloadTask* task);

 private:
  friend class web::LazyWebStateUserData<DriveTabHelper>;
  explicit DriveTabHelper(web::WebState* web_state);

  // web::DownloadTaskObserver overrides:
  void OnDownloadUpdated(web::DownloadTask* task) override;
  void OnDownloadDestroyed(web::DownloadTask* task) override;

  // Resets `download_task_obs_` and `upload_task_`. If `task` and `identity`
  // are non-nil, `task` will be observed and a new upload task will be created
  // with identity.
  void ResetSaveToDriveData(web::DownloadTask* task,
                            id<SystemIdentity> identity);

  // Associated WebState.
  raw_ptr<web::WebState> web_state_;

  // Scoped observation to observe the `DownloadTask`.
  using ScopedDownloadTaskObservation =
      base::ScopedObservation<web::DownloadTask, web::DownloadTaskObserver>;
  ScopedDownloadTaskObservation download_task_obs_{this};
  // Drive upload task associated with the observed download task. Should be
  // started as soon as the download task is completed.
  std::unique_ptr<DriveUploadTask> upload_task_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_TAB_HELPER_H_
