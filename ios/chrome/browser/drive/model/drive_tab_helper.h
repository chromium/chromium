// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_TAB_HELPER_H_

#import "base/scoped_observation.h"
#import "ios/chrome/browser/drive/model/download_task_save_to_drive_data.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// Manages Save to Drive tab-scoped state i.e. if a `DownloadTask` is received
// through AddDownloadToSaveToDrive(...) then this tab helper
// - 1 - memorises the task, what `SystemIdentity` will be used to save the
// downloaded file to Drive, etc.
// - 2 - observes the `DownloadTask` and uploads the downloaded file using the
// Drive service upon completion of the task.
class DriveTabHelper : public web::WebStateUserData<DriveTabHelper>,
                       public web::DownloadTaskObserver {
 public:
  DriveTabHelper(const DriveTabHelper&) = delete;
  DriveTabHelper& operator=(const DriveTabHelper&) = delete;
  ~DriveTabHelper() override;

  // Adds a DownloadTask to Save to Drive. This download task will be observed
  // and the downloaded file will be uploaded to Drive.
  void AddDownloadToSaveToDrive(web::DownloadTask* task,
                                id<SystemIdentity> identity);
  // Returns Save to Drive data associated with the current download task.
  std::optional<DownloadTaskSaveToDriveData> GetDownloadTaskSaveToDriveData()
      const;

 private:
  friend class web::WebStateUserData<DriveTabHelper>;
  explicit DriveTabHelper(web::WebState* web_state);

  // web::DownloadTaskObserver overrides:
  void OnDownloadUpdated(web::DownloadTask* task) override;
  void OnDownloadDestroyed(web::DownloadTask* task) override;

  // Resets the Save to Drive data i.e. stop observing the current task, and
  // start observing the task in `data` if any.
  void ResetSaveToDriveData(std::optional<DownloadTaskSaveToDriveData> data);

  // Associated WebState.
  raw_ptr<web::WebState> web_state_;

  // Save to Drive data associated with the current download task.
  std::optional<DownloadTaskSaveToDriveData> download_task_save_to_drive_data_;
  // Scoped observation to observe the `DownloadTask`.
  using ScopedDownloadTaskObservation =
      base::ScopedObservation<web::DownloadTask, web::DownloadTaskObserver>;
  ScopedDownloadTaskObservation download_task_obs_{this};

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_TAB_HELPER_H_
