// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_SAFARI_DOWNLOAD_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_SAFARI_DOWNLOAD_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#include "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol SafariDownloadTabHelperDelegate;
namespace web {
class DownloadTask;
class WebState;
}  // namespace web

// TabHelper that manages files downloaded with SFSafariViewController.
class SafariDownloadTabHelper
    : public web::WebStateUserData<SafariDownloadTabHelper> {
 public:
  SafariDownloadTabHelper(const SafariDownloadTabHelper&) = delete;
  SafariDownloadTabHelper& operator=(const SafariDownloadTabHelper&) = delete;

  id<SafariDownloadTabHelperDelegate> delegate() { return delegate_; }

  // `delegate` is not retained by this instance.
  void set_delegate(id<SafariDownloadTabHelperDelegate> delegate) {
    delegate_ = delegate;
  }

  // SafariDownloadTabHelper does not really proceed with the download. Instead
  // it extract the download URL and forward it to SFSafariViewController. The
  // task is dropped and destroyed at the end of the method.
  void DownloadMobileConfig(std::unique_ptr<web::DownloadTask> task);
  void DownloadCalendar(std::unique_ptr<web::DownloadTask> task);

 private:
  friend class web::WebStateUserData<SafariDownloadTabHelper>;

  explicit SafariDownloadTabHelper(web::WebState* web_state);

  __weak id<SafariDownloadTabHelperDelegate> delegate_ = nil;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_SAFARI_DOWNLOAD_TAB_HELPER_H_
