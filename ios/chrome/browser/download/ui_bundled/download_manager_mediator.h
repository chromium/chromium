// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_MEDIATOR_H_

#import "base/files/file_path.h"
#import "base/memory/weak_ptr.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/drive/model/upload_task_observer.h"
#import "ios/chrome/browser/download/ui_bundled/download_manager_consumer.h"
#import "ios/web/public/download/download_task_observer.h"

@protocol DownloadManagerConsumer;

namespace drive {
class DriveService;
}

class PrefService;
class UploadTask;

namespace signin {
class IdentityManager;
}

namespace web {
class DownloadTask;
}  // namespace web

// Manages a single download task by providing means to start the download and
// update consumer if download task was changed.
class DownloadManagerMediator : public web::DownloadTaskObserver,
                                public UploadTaskObserver,
                                public signin::IdentityManager::Observer {
 public:
  DownloadManagerMediator();

  DownloadManagerMediator(const DownloadManagerMediator&) = delete;
  DownloadManagerMediator& operator=(const DownloadManagerMediator&) = delete;

  ~DownloadManagerMediator() override;

  // Sets whether this download manager is in an Incognito browser.
  void SetIsIncognito(bool is_incognito);

  // Sets the identity manager.
  void SetIdentityManager(signin::IdentityManager* identity_manager);

  // Sets the Drive service.
  void SetDriveService(drive::DriveService* drive_service);

  // Sets the pref service.
  void SetPrefService(PrefService* pref_service);

  // Sets download manager consumer. Not retained by mediator.
  void SetConsumer(id<DownloadManagerConsumer> consumer);

  // Sets download task. Must be set to null when task is destroyed.
  void SetDownloadTask(web::DownloadTask* task);

  // Returns the path of the downloaded file after download is completed.
  // Returns empty path otherwise (f.e. download is still in progress).
  base::FilePath GetDownloadPath();

  // Returns the current upload task, if any.
  UploadTask* GetUploadTask();

  // Asynchronously starts download operation.
  void StartDownloading();

  // Converts web::DownloadTask::State to DownloadManagerState.
  DownloadManagerState GetDownloadManagerState() const;

  // Returns whether Drive should be presented as a destination for downloads.
  bool IsSaveToDriveAvailable() const;

  // Updates consumer.
  void UpdateConsumer();

  // Informs the consumer that the Google Drive app is installed.
  void SetGoogleDriveAppInstalled(bool installed);

  // Start/stop listening for foregrounding notifications.
  void StartObservingNotifications();
  void StopObservingNotifications();

 private:
  // Moves the downloaded file to user's Documents if it exists.
  void MoveToUserDocumentsIfFileExists(base::FilePath task_path,
                                       bool file_exists);

  // Removes the downloaded file if it exists.
  void RemoveIfFileExists(base::FilePath task_path, bool file_exists);

  // Checks if the move has been completed.
  void MoveComplete(bool move_completed);

  // Checks if the remove has been completed.
  void RemoveComplete(bool remove_completed);

  // Converts DownloadTask progress [0;100] to float progress [0.0f;1.0f].
  float GetDownloadManagerProgress() const;

  // Returns accessibility announcement for download state change. -1 if there
  // is no announcement.
  int GetDownloadManagerA11yAnnouncement() const;

  // Finds any upload task associated with the current download task and calls
  // `SetUploadTask()` accordingly.
  void UpdateUploadTask();
  // Sets upload task. Must be set to null when task is destroyed.
  void SetUploadTask(UploadTask* task);

  // web::DownloadTaskObserver overrides:
  void OnDownloadUpdated(web::DownloadTask* task) override;
  void OnDownloadDestroyed(web::DownloadTask* task) override;

  // UploadTaskObserver overrides:
  void OnUploadUpdated(UploadTask* task) override;
  void OnUploadDestroyed(UploadTask* task) override;

  // signin::IdentityManager::Observer overrides:
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  void AppWillEnterForeground();

  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  raw_ptr<drive::DriveService> drive_service_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;
  bool is_incognito_;
  base::FilePath download_path_;
  raw_ptr<web::DownloadTask> download_task_ = nullptr;
  raw_ptr<UploadTask> upload_task_ = nullptr;
  __weak id<DownloadManagerConsumer> consumer_ = nil;
  // Observers for NSNotificationCenter notifications.
  __strong id<NSObject> application_foregrounding_observer_;
  bool is_google_drive_app_installed_ = false;

  base::WeakPtrFactory<DownloadManagerMediator> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_MEDIATOR_H_
