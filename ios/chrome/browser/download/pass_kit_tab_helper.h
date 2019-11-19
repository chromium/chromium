// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_PASS_KIT_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_PASS_KIT_TAB_HELPER_H_

#include <memory>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol PassKitTabHelperDelegate;
namespace web {
class DownloadTask;
class WebState;
}  // namespace web

// Key of the UMA Download.IOSDownloadPassKitResult histogram.
extern const char kUmaDownloadPassKitResult[];

// Enum for the Download.IOSDownloadPassKitResult UMA histogram to report the
// results of the PassKit download.
// Note: This enum is used to back an UMA histogram, and should be treated as
// append-only.
enum class DownloadPassKitResult {
  Successful = 0,
  // PassKit download failed for a reason other than wrong MIME type or 401/403
  // HTTP response.
  OtherFailure,
  // PassKit download failed due to either a 401 or 403 HTTP response.
  UnauthorizedFailure,
  // PassKit download did not download the correct MIME type. This can happen
  // when web server redirects to login page instead of returning PassKit data.
  WrongMimeTypeFailure,
  Count
};

// TabHelper which downloads pkpass file, constructs PKPass object and passes
// that PKPass to the delegate.
class PassKitTabHelper : public web::WebStateUserData<PassKitTabHelper>,
                         public web::DownloadTaskObserver {
 public:
  ~PassKitTabHelper() override;

  // Creates TabHelper. |delegate| is not retained by TabHelper. |web_state|
  // must not be null.
  static void CreateForWebState(web::WebState* web_state,
                                id<PassKitTabHelperDelegate> delegate);

  // Asynchronously downloads pkpass file using the given |task|. Asks delegate
  // to present "Add pkpass" dialog when the download is complete.
  virtual void Download(std::unique_ptr<web::DownloadTask> task);

 protected:
  // Allow subclassing from PassKitTabHelper for testing purposes.
  PassKitTabHelper(web::WebState* web_state,
                   id<PassKitTabHelperDelegate> delegate);

 private:
  friend class web::WebStateUserData<PassKitTabHelper>;

  // web::DownloadTaskObserver overrides:
  void OnDownloadUpdated(web::DownloadTask* task) override;

  web::WebState* web_state_;
  __weak id<PassKitTabHelperDelegate> delegate_ = nil;
  // Set of unfinished download tasks.
  std::set<std::unique_ptr<web::DownloadTask>, base::UniquePtrComparator>
      tasks_;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PassKitTabHelper);
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_PASS_KIT_TAB_HELPER_H_
