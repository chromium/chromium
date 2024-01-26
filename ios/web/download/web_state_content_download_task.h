// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_WEB_STATE_CONTENT_DOWNLOAD_TASK_H_
#define IOS_WEB_DOWNLOAD_WEB_STATE_CONTENT_DOWNLOAD_TASK_H_

#include "base/task/sequenced_task_runner.h"
#include "ios/web/download/download_task_impl.h"
#import "ios/web/public/download/crw_web_view_download.h"

@class WebStateDownloadDelegateBridge;

namespace web {

// Implementation of DownloadTaskImpl that download the content of the web
// state.
class WebStateContentDownloadTask final : public DownloadTaskImpl {
 public:
  // Constructs a new WebStateContentDownloadTask objects. `web_state` and
  // `identifier` must be valid.
  WebStateContentDownloadTask(
      WebState* web_state,
      const GURL& original_url,
      NSString* http_method,
      const std::string& content_disposition,
      int64_t total_bytes,
      const std::string& mime_type,
      NSString* identifier,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  WebStateContentDownloadTask(const WebStateContentDownloadTask&) = delete;
  WebStateContentDownloadTask& operator=(const WebStateContentDownloadTask&) =
      delete;

  ~WebStateContentDownloadTask() final;

  // DownloadTaskImpl overrides:
  void StartInternal(const base::FilePath& path) final;

  // Cancel the downloads.
  // Local downloads (it the webState is showing a file:// URL) cannot be
  // cancelled.
  void CancelInternal() final;

 private:
  // Called when the download finishes.
  void DownloadDidFinish(NSError* error);

  // Called when the WebState created the download object.
  void DownloadWasCreated(id<CRWWebViewDownload> download);

  // The download object returned by the web_state used to cancel the download.
  id<CRWWebViewDownload> download_;

  // Bridge to the CRWWebViewDownloadDelegate.
  WebStateDownloadDelegateBridge* download_delegate_;

  base::WeakPtrFactory<WebStateContentDownloadTask> weak_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_DOWNLOAD_WEB_STATE_CONTENT_DOWNLOAD_TASK_H_
