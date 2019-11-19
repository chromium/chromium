// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_TAB_HELPER_H_

#include <memory>

#import <Foundation/Foundation.h>

#include "base/macros.h"
#include "ios/web/public/download/download_task_observer.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol DownloadManagerTabHelperDelegate;
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
  ~DownloadManagerTabHelper() override;

  // Creates TabHelper. |delegate| is not retained by TabHelper. |web_state|
  // must not be null.
  static void CreateForWebState(web::WebState* web_state,
                                id<DownloadManagerTabHelperDelegate> delegate);

  // Asynchronously downloads a file using the given |task|.
  virtual void Download(std::unique_ptr<web::DownloadTask> task);

  // Returns |true| after Download() was called, |false| after the task was
  // cancelled.
  bool has_download_task() const { return task_.get(); }

 protected:
  // Allow subclassing from DownloadManagerTabHelper for testing purposes.
  DownloadManagerTabHelper(web::WebState* web_state,
                           id<DownloadManagerTabHelperDelegate> delegate);

 private:
  friend class web::WebStateUserData<DownloadManagerTabHelper>;

  // web::WebStateObserver overrides:
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // web::DownloadTaskObserver overrides:
  void OnDownloadUpdated(web::DownloadTask* task) override;

  // Returns key for using with NetworkActivityIndicatorManager.
  NSString* GetNetworkActivityKey() const;

  // Assigns |task| to |task_|; replaces the current download if exists;
  // instructs the delegate that download has started.
  void DidCreateDownload(std::unique_ptr<web::DownloadTask> task);

  web::WebState* web_state_ = nullptr;
  __weak id<DownloadManagerTabHelperDelegate> delegate_ = nil;
  std::unique_ptr<web::DownloadTask> task_;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerTabHelper);
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_TAB_HELPER_H_
