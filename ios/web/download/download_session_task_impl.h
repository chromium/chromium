// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DOWNLOAD_SESSION_TASK_IMPL_H_
#define IOS_WEB_DOWNLOAD_DOWNLOAD_SESSION_TASK_IMPL_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "ios/web/download/download_task_impl.h"

namespace web {
namespace download {
namespace internal {
class Session;
class TaskInfo;
}  // namespace internal
}  // namespace download

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

  // Constructs a new DownloadSessionTaskImpl objects. `web_state`, `identifier`
  // and `delegate` must be valid.
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

  // DownloadTaskImpl overrides:
  void StartInternal(const base::FilePath& path) final;
  void CancelInternal() final;

 private:
  friend class download::internal::Session;

  // Called when the file has been created.
  void OnFileCreated(base::File file);

  // Called when the cookies has been fetched.
  void OnCookiesFetched(base::File file, NSArray<NSHTTPCookie*>* cookies);

  // Called when information about the download is received from the
  // background NSURLSessionTask.
  void ApplyTaskInfo(download::internal::TaskInfo task_info);

  // Called when data has been written to disk.
  void OnDataWritten(int64_t data_size);

  // Recompute the completion percentage from received bytes.
  void RecomputePercentCompleted();

  SessionFactory session_factory_;
  std::unique_ptr<download::internal::Session> session_;

  base::WeakPtrFactory<DownloadSessionTaskImpl> weak_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_DOWNLOAD_DOWNLOAD_SESSION_TASK_IMPL_H
