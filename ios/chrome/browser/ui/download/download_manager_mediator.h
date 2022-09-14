// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_MEDIATOR_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#import "ios/chrome/browser/ui/download/download_manager_consumer.h"
#include "ios/web/public/download/download_task_observer.h"

@protocol DownloadManagerConsumer;

namespace web {
class DownloadTask;
}  // namespace web

// Manages a single download task by providing means to start the download and
// update consumer if download task was changed.
class DownloadManagerMediator : public web::DownloadTaskObserver {
 public:
  DownloadManagerMediator();

  DownloadManagerMediator(const DownloadManagerMediator&) = delete;
  DownloadManagerMediator& operator=(const DownloadManagerMediator&) = delete;

  ~DownloadManagerMediator() override;

  // Sets download manager consumer. Not retained by mediator.
  void SetConsumer(id<DownloadManagerConsumer> consumer);

  // Sets download task. Must be set to null when task is destroyed.
  void SetDownloadTask(web::DownloadTask* task);

  // Returns the path of the downloaded file after download is completed.
  // Returns empty path otherwise (f.e. download is still in progress).
  base::FilePath GetDownloadPath();

  // Asynchronously starts download operation.
  void StartDowloading();

 private:
  // Updates consumer from web::DownloadTask.
  void UpdateConsumer();

  // Moves the downloaded file to user's Documents if it exists.
  void MoveToUserDocumentsIfFileExists(base::FilePath download_path,
                                       bool file_exists);

  // Restores the download path once the downloaded file has been moved to
  // user's Documents.
  void RestoreDownloadPath(base::FilePath user_download_path,
                           bool moveCompleted);

  // Converts web::DownloadTask::State to DownloadManagerState.
  DownloadManagerState GetDownloadManagerState() const;

  // Converts DownloadTask progress [0;100] to float progress [0.0f;1.0f].
  float GetDownloadManagerProgress() const;

  // Returns accessibility announcement for download state change. -1 if there
  // is no announcement.
  int GetDownloadManagerA11yAnnouncement() const;

  // web::DownloadTaskObserver overrides:
  void OnDownloadUpdated(web::DownloadTask* task) override;
  void OnDownloadDestroyed(web::DownloadTask* task) override;

  base::FilePath download_path_;
  web::DownloadTask* task_ = nullptr;
  __weak id<DownloadManagerConsumer> consumer_ = nil;
  base::WeakPtrFactory<DownloadManagerMediator> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_MEDIATOR_H_
