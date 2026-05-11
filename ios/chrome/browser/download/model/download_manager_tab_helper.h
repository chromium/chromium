// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_MANAGER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_MANAGER_TAB_HELPER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ios/chrome/browser/download/model/download_manager_tab_helper_delegate.h"
#include "ios/web/public/download/download_task_observer.h"
#include "ios/web/public/web_state_observer.h"
#include "ios/web/public/web_state_user_data.h"

@protocol SnackbarCommands;

class DownloadFileService;

namespace enterprise_connectors {
class IOSAnalysisRequestHandler;
}

namespace web {
class DownloadTask;
class WebState;
}  // namespace web

// TabHelper which manages a single file download.
class DownloadManagerTabHelper
    : public web::WebStateUserData<DownloadManagerTabHelper>,
      public web::WebStateObserver,
      public web::DownloadTaskObserver {
 public:
  DownloadManagerTabHelper(const DownloadManagerTabHelper&) = delete;
  DownloadManagerTabHelper& operator=(const DownloadManagerTabHelper&) = delete;

  ~DownloadManagerTabHelper() override;

  // Returns whether downloads should be restricted. It checks if downloads
  // should be restricted based on the download restriction policy for files,
  // save to drive policy, and incognito.
  static bool ShouldRestrictDownload(web::WebState* web_state);

  // Returns whether downloads to file should be restricted. It checks if
  // downloads should be restricted based on the download restriction policy.
  static bool ShouldRestrictDownloadToFile(web::WebState* web_state);

  // Set the current download task for this tab.
  virtual void SetCurrentDownload(std::unique_ptr<web::DownloadTask> task);

  // Returns the final file path for the current download if stored locally.
  const base::FilePath& GetDownloadTaskFinalFilePath() const;

  // Returns `true` after Download() was called, `false` after the task was
  // cancelled.
  bool has_download_task() const { return task_.get(); }

  // Returns the currently active download task.
  web::DownloadTask* GetActiveDownloadTask();

  // Sets the delegate. The tab helper will no-op if the delegate is nil.
  void SetDelegate(id<DownloadManagerTabHelperDelegate> delegate);

  // Sets the snackbar handler.
  void SetSnackbarHandler(id<SnackbarCommands> snackbar_handler);

  // Starts the current download task. Asserts that `task == task_`.
  virtual void StartDownload(web::DownloadTask* task);

  // Cleans up current download resources if any and notifies delegate.
  void CleanupCurrentDownload();

  // Sets whether the Download toolbar should adapt to the fullscreen state.
  virtual void AdaptToFullscreen(bool adapt_to_fullscreen);

  // Returns whether `task_` still needs to be saved to Drive.
  bool WillDownloadTaskBeSavedToDrive() const;

 protected:
  // Allow subclassing from DownloadManagerTabHelper for testing purposes.
  explicit DownloadManagerTabHelper(web::WebState* web_state);

 private:
  friend class web::WebStateUserData<DownloadManagerTabHelper>;

  // web::WebStateObserver overrides:
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // web::DownloadTaskObserver overrides:
  void OnDownloadUpdated(web::DownloadTask* task) override;

  // Assigns `task` to `task_`; replaces the current download if exists;
  // instructs the delegate that download has started.
  void DidCreateDownload(std::unique_ptr<web::DownloadTask> task);

  // When a new download is started while another is in progress, the delegate
  // is queried to know how to proceed. This method is passed as callback to
  // the delegate and is invoked with the decision made by the user.
  void OnDownloadPolicyDecision(std::unique_ptr<web::DownloadTask> task,
                                NewDownloadPolicy policy);

  // Displays a snackbar when download is restricted.
  void ShowRestrictDownloadSnackbar();

  // Use `user_documents_path` as the download file destination.
  void UseAvailableUserDocumentsPath(base::FilePath user_documents_path);

  // Moves the downloaded file to user's Documents if it exists.
  void MoveToUserDocumentsIfFileExists(base::FilePath task_path,
                                       bool file_exists);

  // Called when the file move operation completes.
  void MoveComplete(bool move_completed,
                    const std::string& download_id,
                    const base::FilePath& source_path,
                    const base::FilePath& final_path);

  // Begins the Auto-deletion enrollment process for the given task if enabled.
  void MaybeEnrollFileForAutoDeletion(web::DownloadTask* task);

  // Sets the download path for Auto-deletion if enabled.
  void MaybeSetDownloadPathForAutoDeletion();

  // Move the download to user selected location if `shouldProceed` is set as
  // true, otherwise clean up the current download task.
  void MaybeMoveDownloadToDownloadsDirectory(bool shouldProceed);

  // Process the complete download task. Move the download item to the user
  // selected location if it's not to be saved to google drive, otherwise stop
  // the process.
  void ProcessCompleteDownloadTask();

  // Returns the DownloadFileService instance.
  DownloadFileService* GetDownloadFileService();

  raw_ptr<web::WebState> web_state_ = nullptr;
  __weak id<DownloadManagerTabHelperDelegate> delegate_ = nil;
  __weak id<SnackbarCommands> snackbar_handler_ = nil;
  std::unique_ptr<web::DownloadTask> task_;
  std::unique_ptr<enterprise_connectors::IOSAnalysisRequestHandler>
      analysis_request_handler_;
  base::FilePath task_final_file_path_;
  bool delegate_started_ = false;

  base::WeakPtrFactory<DownloadManagerTabHelper> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_MANAGER_TAB_HELPER_H_
