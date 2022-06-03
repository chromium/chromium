// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MOBILECONFIG_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MOBILECONFIG_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#include "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol MobileConfigTabHelperDelegate;
namespace web {
class DownloadTask;
class WebState;
}  // namespace web

// TabHelper which manages .mobileconfig files.
class MobileConfigTabHelper
    : public web::WebStateUserData<MobileConfigTabHelper> {
 public:
  MobileConfigTabHelper() = default;
  MobileConfigTabHelper(const MobileConfigTabHelper&) = delete;
  MobileConfigTabHelper& operator=(const MobileConfigTabHelper&) = delete;

  // Creates TabHelper. |web_state| must not be null.
  static void CreateForWebState(web::WebState* web_state);

  id<MobileConfigTabHelperDelegate> delegate() { return delegate_; }

  // |delegate| is not retained by this instance.
  void set_delegate(id<MobileConfigTabHelperDelegate> delegate) {
    delegate_ = delegate;
  }

  // Download the .mobileconfig file using SFSafariViewController and stops the
  // given |task|.
  void Download(std::unique_ptr<web::DownloadTask> task);

 private:
  friend class web::WebStateUserData<MobileConfigTabHelper>;
  __weak id<MobileConfigTabHelperDelegate> delegate_ = nil;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MOBILECONFIG_TAB_HELPER_H_
