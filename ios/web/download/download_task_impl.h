// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DOWNLOAD_TASK_IMPL_H_
#define IOS_WEB_DOWNLOAD_DOWNLOAD_TASK_IMPL_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "ios/web/download/download_result.h"
#include "ios/web/public/download/download_task.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace web {
namespace download {
namespace internal {
struct CreateFileResult;
}  // namespace internal
}  // namespace download

class DownloadTaskObserver;
class WebState;

// Partial implementation of the DownloadTask interface used to share common
// behaviour between the different concrete sub-classes.
class DownloadTaskImpl : public DownloadTask {
 public:
  // Constructs a new DownloadTaskImpl objects. `web_state` and `identifier`
  // must be valid.
  DownloadTaskImpl(WebState* web_state,
                   const GURL& original_url,
                   NSString* http_method,
                   const std::string& content_disposition,
                   int64_t total_bytes,
                   const std::string& mime_type,
                   NSString* identifier,
                   const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  ~DownloadTaskImpl() override;

  // DownloadTask overrides:
  WebState* GetWebState() final;
  DownloadTask::State GetState() const final;
  void Start(const base::FilePath& path) final;
  void Cancel() final;
  NSString* GetIdentifier() const final;
  const GURL& GetOriginalUrl() const final;
  NSString* GetHttpMethod() const final;
  bool IsDone() const final;
  int GetErrorCode() const final;
  int GetHttpCode() const final;
  int64_t GetTotalBytes() const final;
  int64_t GetReceivedBytes() const final;
  int GetPercentComplete() const final;
  std::string GetContentDisposition() const final;
  std::string GetOriginalMimeType() const final;
  std::string GetMimeType() const final;
  base::FilePath GenerateFileName() const final;
  bool HasPerformedBackgroundDownload() const final;
  void AddObserver(DownloadTaskObserver* observer) final;
  void RemoveObserver(DownloadTaskObserver* observer) final;
  void GetResponseData(ResponseDataReadCallback callback) const final;
  const base::FilePath& GetResponsePath() const final;

 private:
  // Needs to be overridden by sub-classes to perform the download. When this
  // method is invoked, `path` is non-empty, its parent directory exists, the
  // location is writable, but the file does not exist.
  virtual void StartInternal(const base::FilePath& path) = 0;

  // Needs to be overridden by sub-classes to clean themselves when the
  // download is cancelled. If they need to clean during their shutdown,
  // then they need to call `CancelInternal()` from their destructor.
  virtual void CancelInternal() = 0;

  // Can be overridden by sub-classes to return a suggested name for the
  // downloaded file. The default implementation returns an empty string.
  virtual std::string GetSuggestedName() const;

  // Invoked when UIApplicationWillResignActiveNotification is received.
  void OnAppWillResignActive();

  // Invoked asynchronously when the file has been created.
  void OnDownloadFileCreated(download::internal::CreateFileResult result);

 protected:
  // Called when download was completed and the data writing was finished.
  void OnDownloadFinished(DownloadResult download_result);

  // Called when download task was updated.
  void OnDownloadUpdated();

  // Used to check that the methods are called on the correct sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // A list of observers. Weak references.
  base::ObserverList<DownloadTaskObserver, true> observers_;

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
  raw_ptr<WebState> web_state_ = nullptr;

  base::FilePath path_;
  bool owns_file_ = false;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Observes UIApplicationWillResignActiveNotification notifications.
  id<NSObject> observer_ = nil;

  base::WeakPtrFactory<DownloadTaskImpl> weak_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_DOWNLOAD_DOWNLOAD_TASK_IMPL_H_
