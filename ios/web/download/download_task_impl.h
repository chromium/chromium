// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DOWNLOAD_TASK_IMPL_H_
#define IOS_WEB_DOWNLOAD_DOWNLOAD_TASK_IMPL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#import "ios/web/public/download/download_task.h"
#include "url/gurl.h"

@class NSURLSession;

namespace net {
class URLFetcherResponseWriter;
}

namespace web {

class DownloadTaskObserver;
class WebState;

// Implements DownloadTask interface. Uses background NSURLSession as
// implementation.
class DownloadTaskImpl : public DownloadTask {
 public:
  class Delegate {
   public:
    // Called when download task is about to be destroyed. Delegate should
    // remove all references to the given DownloadTask and stop using it.
    virtual void OnTaskDestroyed(DownloadTaskImpl* task) = 0;

    // Creates background NSURLSession with given |identifier|, |delegate| and
    // |delegate_queue|.
    virtual NSURLSession* CreateSession(NSString* identifier,
                                        id<NSURLSessionDataDelegate> delegate,
                                        NSOperationQueue* delegate_queue) = 0;
    virtual ~Delegate() = default;
  };

  // Constructs a new DownloadTaskImpl objects. |web_state|, |identifier| and
  // |delegate| must be valid.
  DownloadTaskImpl(const WebState* web_state,
                   const GURL& original_url,
                   const std::string& content_disposition,
                   int64_t total_bytes,
                   const std::string& mime_type,
                   ui::PageTransition page_transition,
                   NSString* identifier,
                   Delegate* delegate);

  // Stops the download operation and clears the delegate.
  void ShutDown();

  // DownloadTask overrides:
  DownloadTask::State GetState() const override;
  void Start(std::unique_ptr<net::URLFetcherResponseWriter> writer) override;
  void Cancel() override;
  net::URLFetcherResponseWriter* GetResponseWriter() const override;
  NSString* GetIndentifier() const override;
  const GURL& GetOriginalUrl() const override;
  bool IsDone() const override;
  int GetErrorCode() const override;
  int GetHttpCode() const override;
  int64_t GetTotalBytes() const override;
  int64_t GetReceivedBytes() const override;
  int GetPercentComplete() const override;
  std::string GetContentDisposition() const override;
  std::string GetMimeType() const override;
  ui::PageTransition GetTransitionType() const override;
  base::string16 GetSuggestedFilename() const override;
  bool HasPerformedBackgroundDownload() const override;
  void AddObserver(DownloadTaskObserver* observer) override;
  void RemoveObserver(DownloadTaskObserver* observer) override;
  ~DownloadTaskImpl() override;

 private:
  // Creates background NSURLSession with given |identifier|.
  NSURLSession* CreateSession(NSString* identifier);

  // Asynchronously returns cookies for WebState associated with this task (on
  // iOS 10 and earlier, the array is always empty as it is not possible to
  // access the cookies). Must be called on UI thread. The callback will be
  // invoked on the UI thread.
  void GetCookies(base::Callback<void(NSArray<NSHTTPCookie*>*)> callback);

  // Asynchronously returns cookies for WebState associated with this task. Must
  // be called on UI thread. The callback will be invoked on the UI thread.
  void GetWKCookies(base::Callback<void(NSArray<NSHTTPCookie*>*)> callback)
      API_AVAILABLE(ios(11.0));

  // Starts the download with given cookies.
  void StartWithCookies(NSArray<NSHTTPCookie*>* cookies);

  // Starts parsing data:// url. Separate code path is used because
  // NSURLSession does not support data URLs.
  void StartDataUrlParsing();

  // Called when download task was updated.
  void OnDownloadUpdated();

  // Called when download was completed and the data writing was finished.
  void OnDownloadFinished(int error_code);

  // Called when data:// url parsing has completed and the data has been
  // written.
  void OnDataUrlWritten(int bytes_written);

  // A list of observers. Weak references.
  base::ObserverList<DownloadTaskObserver, true>::Unchecked observers_;

  // Back up corresponding public methods of DownloadTask interface.
  State state_ = State::kNotStarted;
  std::unique_ptr<net::URLFetcherResponseWriter> writer_;
  GURL original_url_;
  int error_code_ = 0;
  int http_code_ = -1;
  int64_t total_bytes_ = -1;
  int64_t received_bytes_ = 0;
  int percent_complete_ = -1;
  std::string content_disposition_;
  std::string mime_type_;
  ui::PageTransition page_transition_ = ui::PAGE_TRANSITION_LINK;
  NSString* identifier_ = nil;
  bool has_performed_background_download_ = false;

  const WebState* web_state_ = nullptr;
  Delegate* delegate_ = nullptr;
  NSURLSession* session_ = nil;
  NSURLSessionTask* session_task_ = nil;

  // Observes UIApplicationWillResignActiveNotification notifications.
  id<NSObject> observer_ = nil;

  base::WeakPtrFactory<DownloadTaskImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadTaskImpl);
};

}  // namespace web

#endif  // IOS_WEB_DOWNLOAD_DOWNLOAD_TASK_IMPL_H_
