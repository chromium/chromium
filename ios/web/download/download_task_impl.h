// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DOWNLOAD_TASK_IMPL_H_
#define IOS_WEB_DOWNLOAD_DOWNLOAD_TASK_IMPL_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "ios/web/download/download_result.h"
#include "ios/web/public/download/download_task.h"
#include "url/gurl.h"

@class NSURLSession;

namespace web {

class DownloadTaskObserver;
class WebState;

// Partial implementation of the DownloadTask interface used to share common
// behaviour between the different concrete sub-classes.
class DownloadTaskImpl : public DownloadTask {
 public:
  class Delegate {
   public:
    // Called when download task is about to be destroyed. Delegate should
    // remove all references to the given DownloadTask and stop using it.
    virtual void OnTaskDestroyed(DownloadTaskImpl* task) = 0;

    // Creates background NSURLSession with given |identifier|, |cookies|,
    // |delegate| and |delegate_queue|.
    virtual NSURLSession* CreateSession(NSString* identifier,
                                        NSArray<NSHTTPCookie*>* cookies,
                                        id<NSURLSessionDataDelegate> delegate,
                                        NSOperationQueue* delegate_queue) = 0;
    virtual ~Delegate() = default;
  };

  // Constructs a new DownloadTaskImpl objects. |web_state|, |identifier| and
  // |delegate| must be valid.
  DownloadTaskImpl(WebState* web_state,
                   const GURL& original_url,
                   NSString* http_method,
                   const std::string& content_disposition,
                   int64_t total_bytes,
                   const std::string& mime_type,
                   NSString* identifier,
                   Delegate* delegate);

  // Stops the download operation and clears the delegate.
  virtual void ShutDown();

  // DownloadTask overrides:
  WebState* GetWebState() override;
  DownloadTask::State GetState() const override;
  void Start(const base::FilePath& path, Destination destination_hint) override;
  void Cancel() override;
  NSString* GetIndentifier() const override;
  const GURL& GetOriginalUrl() const override;
  NSString* GetHttpMethod() const override;
  bool IsDone() const override;
  int GetErrorCode() const override;
  int GetHttpCode() const override;
  int64_t GetTotalBytes() const override;
  int64_t GetReceivedBytes() const override;
  int GetPercentComplete() const override;
  std::string GetContentDisposition() const override;
  std::string GetOriginalMimeType() const override;
  std::string GetMimeType() const override;
  std::u16string GetSuggestedFilename() const override;
  bool HasPerformedBackgroundDownload() const override;
  void AddObserver(DownloadTaskObserver* observer) override;
  void RemoveObserver(DownloadTaskObserver* observer) override;

  DownloadTaskImpl(const DownloadTaskImpl&) = delete;
  DownloadTaskImpl& operator=(const DownloadTaskImpl&) = delete;

  ~DownloadTaskImpl() override;

 protected:
  // Called when download was completed and the data writing was finished.
  void OnDownloadFinished(DownloadResult download_result);

  // Called when download task was updated.
  void OnDownloadUpdated();

  // Used to check that the methods are called on the correct sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // A list of observers. Weak references.
  base::ObserverList<DownloadTaskObserver, true>::Unchecked observers_;

  // Back up corresponding public methods of DownloadTask interface.
  State state_ = State::kNotStarted;
  GURL original_url_;
  NSString* http_method_ = nil;
  int http_code_ = -1;
  int64_t total_bytes_ = -1;
  int64_t received_bytes_ = 0;
  int percent_complete_ = -1;
  std::string content_disposition_;
  std::string original_mime_type_;
  std::string mime_type_;
  NSString* identifier_ = nil;
  bool has_performed_background_download_ = false;
  DownloadResult download_result_;
  WebState* web_state_ = nullptr;
  Delegate* delegate_ = nullptr;

  // Observes UIApplicationWillResignActiveNotification notifications.
  id<NSObject> observer_ = nil;

  base::WeakPtrFactory<DownloadTaskImpl> weak_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_DOWNLOAD_DOWNLOAD_TASK_IMPL_H_
