// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AR_QUICK_LOOK_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AR_QUICK_LOOK_TAB_HELPER_H_

#include <memory>

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/lazy_web_state_user_data.h"

@protocol ARQuickLookTabHelperDelegate;

namespace web {
class DownloadTask;
class WebState;
}  // namespace web

// The UMA Download.IOSDownloadARModelState histogram name.
extern const char kIOSDownloadARModelStateHistogram[];

// The UMA Download.IOSDownloadARModelState histogram suffixes.
extern const char kUsdzMimeTypeHistogramSuffix[];

// Enum for the Download.IOSDownloadARModelState UMA histogram.
// Note: This enum should be appended to only.
enum class IOSDownloadARModelState {
  // AR model download was created but not yet started.
  kCreated = 0,
  // AR model download was started.
  kStarted = 1,
  // AR model download was successful.
  kSuccessful = 2,
  // AR model download failed due to either a 401/403 HTTP response.
  kUnauthorizedFailure = 3,
  // AR model download did not download the correct MIME type. This can happen
  // in the case of web server redirects.
  kWrongMimeTypeFailure = 4,
  // AR model download failed for a reason other than 401/403 HTTP response or
  // incorrect MIME type. Does not include items already counted in the more
  // specific buckets, e.g., UnauthorizedFailure and WrongMimeTypeFailure.
  kOtherFailure = 5,
  kMaxValue = kOtherFailure,
};

// TabHelper to download and preview USDZ format 3D models for AR.
class ARQuickLookTabHelper
    : public web::DownloadTaskObserver,
      public web::LazyWebStateUserData<ARQuickLookTabHelper> {
 public:
  ARQuickLookTabHelper(const ARQuickLookTabHelper&) = delete;
  ARQuickLookTabHelper& operator=(const ARQuickLookTabHelper&) = delete;

  ~ARQuickLookTabHelper() override;

  id<ARQuickLookTabHelperDelegate> delegate() { return delegate_; }

  void set_delegate(id<ARQuickLookTabHelperDelegate> delegate) {
    delegate_ = delegate;
  }

  // Downloads and previews the USDZ file given by `download_task`. Takes
  // ownership of `download_task`.
  virtual void Download(std::unique_ptr<web::DownloadTask> download_task);

 protected:
  // Allow subclassing from ARQuickLookTabHelper for testing purposes.
  explicit ARQuickLookTabHelper(web::WebState* web_state);

 private:
  friend class web::LazyWebStateUserData<ARQuickLookTabHelper>;

  // Previews the downloaded file given by current download task.
  void DidFinishDownload();
  // Stops observing the current download task and resets the reference.
  void RemoveCurrentDownload();

  // web::DownloadTaskObserver:
  void OnDownloadUpdated(web::DownloadTask* download_task) override;

  // Previews the downloaded USDZ file or confirms the download if download has
  // not started.
  void ConfirmOrPreviewDownload(web::DownloadTask* download_task);

  raw_ptr<web::WebState> web_state_ = nullptr;
  __weak id<ARQuickLookTabHelperDelegate> delegate_ = nil;

  // The current download task.
  std::unique_ptr<web::DownloadTask> download_task_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AR_QUICK_LOOK_TAB_HELPER_H_
