// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DOWNLOAD_SESSION_TASK_IMPL_H_
#define IOS_WEB_DOWNLOAD_DOWNLOAD_SESSION_TASK_IMPL_H_

#include "base/callback.h"
#include "base/ios/block_types.h"
#include "ios/web/download/download_task_impl.h"

namespace net {
class URLFetcherResponseWriter;
}

namespace web {

// Implementation of DownloadTaskImpl that uses NSURLRequest to perform the
// download.
class DownloadSessionTaskImpl final : public DownloadTaskImpl {
 public:
  // A repeating callback that can be used to inject a factory to create
  // a NSURLSession* instance. It can be used by unittests that want to
  // create mock NSURLSession* instance.
  using SessionFactory = base::RepeatingCallback<NSURLSession*(
      NSURLSessionConfiguration* configuration,
      id<NSURLSessionDataDelegate> delegate)>;

  // Constructs a new DownloadSessionTaskImpl objects. |web_state|, |identifier|
  // and |delegate| must be valid.
  DownloadSessionTaskImpl(
      WebState* web_state,
      const GURL& original_url,
      NSString* http_method,
      const std::string& content_disposition,
      int64_t total_bytes,
      const std::string& mime_type,
      NSString* identifier,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      SessionFactory session_factory = SessionFactory());

  DownloadSessionTaskImpl(const DownloadSessionTaskImpl&) = delete;
  DownloadSessionTaskImpl& operator=(const DownloadSessionTaskImpl&) = delete;

  ~DownloadSessionTaskImpl() final;

  // DownloadTask overrides:
  NSData* GetResponseData() const final;
  const base::FilePath& GetResponsePath() const final;

  // DownloadTaskImpl overrides:
  void Start(const base::FilePath& path, Destination destination_hint) final;
  void Cancel() final;

 private:
  // Called once net::URLFetcherResponseWriter completes the download
  void OnWriterDownloadFinished(int error_code);

  // Called once the net::URLFetcherResponseWriter created in
  // Start() has been initialised. The download can be started
  // unless the initialisation has failed (as reported by the
  // |writer_initialization_status| result).
  void OnWriterInitialized(
      std::unique_ptr<net::URLFetcherResponseWriter> writer,
      int writer_initialization_status);

  // Creates background NSURLSession with given |identifier| and |cookies|.
  NSURLSession* CreateSession(NSString* identifier,
                              NSArray<NSHTTPCookie*>* cookies);

  // Asynchronously returns cookies for WebState associated with this task.
  // Must be called on UI thread. The callback will be invoked on the UI thread.
  void GetCookies(base::OnceCallback<void(NSArray<NSHTTPCookie*>*)> callback);

  // Starts the download with given cookies.
  void StartWithCookies(NSArray<NSHTTPCookie*>* cookies);

  // Called to implement the method -URLSession:task:didCompleteWithError:
  // from NSURLSessionDataDelegate.
  void OnTaskDone(NSURLSessionTask* task, NSError* error);

  // Called to implement the method -URLSession:dataTask:didReceiveData:
  // from NSURLSessionDataDelegate.
  void OnTaskData(NSURLSessionTask* task,
                  NSData* data,
                  ProceduralBlock completion_handler);

  // Called from either OnTaskData() or OnTaskDone() to update the task
  // progress, and optionally notify the observer of those updates.
  void OnTaskTick(NSURLSessionTask* task, bool notify_download_updated);

  SessionFactory session_factory_;
  std::unique_ptr<net::URLFetcherResponseWriter> writer_;
  NSURLSession* session_ = nil;
  NSURLSessionTask* session_task_ = nil;

  base::WeakPtrFactory<DownloadSessionTaskImpl> weak_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_DOWNLOAD_DOWNLOAD_SESSION_TASK_IMPL_H
