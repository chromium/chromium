// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_SAFARI_DOWNLOAD_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_SAFARI_DOWNLOAD_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#import <optional>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol SafariDownloadTabHelperDelegate;
namespace web {
class DownloadTask;
class WebState;
}  // namespace web

// TabHelper that manages files downloaded with SFSafariViewController.
class SafariDownloadTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<SafariDownloadTabHelper> {
 public:
  SafariDownloadTabHelper(const SafariDownloadTabHelper&) = delete;
  SafariDownloadTabHelper& operator=(const SafariDownloadTabHelper&) = delete;

  ~SafariDownloadTabHelper() override;

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
  void DownloadAppleWalletOrder(std::unique_ptr<web::DownloadTask> task);

 private:
  friend class web::WebStateUserData<SafariDownloadTabHelper>;

  explicit SafariDownloadTabHelper(web::WebState* web_state);

  // web::WebStateObserver overrides:
  void WasShown(web::WebState* web_state) override;

  // Enum representing the type of download triggered using
  // SFSafariViewController.
  enum class PendingType {
    kMobileConfig,
    kCalendar,
    kAppleWalletOrder,
  };

  // Structure to hold the metadata of a Safari download that was
  // completed/triggered while the web state was not active (hidden), allowing
  // it to be deferred.
  struct PendingSafariDownload {
    PendingType type;
    NSURL* url;
  };

  raw_ptr<web::WebState> web_state_ = nullptr;
  __weak id<SafariDownloadTabHelperDelegate> delegate_ = nil;

  // The download triggered while the tab was hidden.
  std::optional<PendingSafariDownload> pending_download_;

  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_SAFARI_DOWNLOAD_TAB_HELPER_H_
