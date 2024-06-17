// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOCUMENT_DOWNLOAD_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOCUMENT_DOWNLOAD_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/download/download_task.h"
#import "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// DocumentDownloadTabHelper that triggers download tasks on non-HTML and
// non-Video files. Video are not supported as they appear in fullscreen an the
// download bar is not visible.
class DocumentDownloadTabHelper
    : public web::WebStateObserver,
      public web::DownloadTaskObserver,
      public web::WebStateUserData<DocumentDownloadTabHelper> {
 public:
  DocumentDownloadTabHelper(const DocumentDownloadTabHelper&) = delete;
  DocumentDownloadTabHelper& operator=(const DocumentDownloadTabHelper&) =
      delete;
  explicit DocumentDownloadTabHelper(web::WebState* web_state);

  ~DocumentDownloadTabHelper() override;

  // Returns whether the current download task was created by this TabHelper.
  bool IsDownloadTaskCreatedByCurrentTabHelper();

  // web::WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;

  // web::DownloadTaskObserver overrides:
  void OnDownloadDestroyed(web::DownloadTask* task) override;
  void OnDownloadUpdated(web::DownloadTask* task) override;

 private:
  friend class web::WebStateUserData<DocumentDownloadTabHelper>;

  // Attach the fullscreen observation to the toolbar UI.
  void AttachFullscreen();

  // Detach the fullscreen observation from the toolbar UI.
  void DetachFullscreen();

  // Used in the case when the task created to download the document is queued.
  // In that case, the current task is observed and `OnPreviousTaskDeleted` is
  // called on the task deletion.
  void OnPreviousTaskDeleted();

  // The webState this TabHelper is attached to.
  raw_ptr<web::WebState> web_state_;

  // Whether the task has been created for the current navigation.
  NSString* task_uuid_ = nil;

  // Whether the download should be triggered but there is already an ongoing
  // task.
  bool waiting_for_previous_task_ = false;

  // Whether `observed_task_` was created by this TabHelper.
  bool current_task_is_document_download_ = false;

  // The download task that is currently observed. Kept for cleanup purpose.
  raw_ptr<web::DownloadTask> observed_task_;

  // The current state of the `observed_task_`.
  // This is used to track the state before it is set to `kCancelled` for
  // destruction.
  web::DownloadTask::State observed_task_state_ =
      web::DownloadTask::State::kNotStarted;

  // The size of the file in bytes, if reported in the response headers.
  int64_t file_size_ = -1;

  WEB_STATE_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<DocumentDownloadTabHelper> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOCUMENT_DOWNLOAD_TAB_HELPER_H_
