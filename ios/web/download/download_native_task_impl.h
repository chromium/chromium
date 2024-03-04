// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DOWNLOAD_NATIVE_TASK_IMPL_H_
#define IOS_WEB_DOWNLOAD_DOWNLOAD_NATIVE_TASK_IMPL_H_

#import <WebKit/WebKit.h>

#include "base/task/sequenced_task_runner.h"
#include "ios/web/download/download_task_impl.h"

@class DownloadNativeTaskBridge;

namespace web {

// Implementation of DownloadTaskImpl that uses WKDownload (wrapped in
// NativeTaskBridge) to perform the download
class DownloadNativeTaskImpl final : public DownloadTaskImpl {
 public:
  // Constructs a new `DownloadNativeTaskImpl` object. `web_state`, `identifier`
  // and `download` must be valid.
  DownloadNativeTaskImpl(
      WebState* web_state,
      const GURL& original_url,
      NSString* http_method,
      const std::string& content_disposition,
      int64_t total_bytes,
      const std::string& mime_type,
      NSString* identifier,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      DownloadNativeTaskBridge* download);

  DownloadNativeTaskImpl(const DownloadNativeTaskImpl&) = delete;
  DownloadNativeTaskImpl& operator=(const DownloadNativeTaskImpl&) = delete;

  ~DownloadNativeTaskImpl() final;

  // DownloadTaskImpl overrides:
  void StartInternal(const base::FilePath& path) final;
  void CancelInternal() final;
  std::string GetSuggestedName() const final;

 private:
  // Invoked when the WKDownload* tasks make progress.
  void OnDownloadProgress(int64_t bytes_received,
                          int64_t total_bytes,
                          double fraction_complete);

  // Invoked when the NSURLResponse of WKDownload is received.
  void OnResponseReceived(int http_error_code, NSString* mime_type);

  DownloadNativeTaskBridge* download_bridge_ = nil;

  base::WeakPtrFactory<DownloadNativeTaskImpl> weak_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_DOWNLOAD_DOWNLOAD_NATIVE_TASK_IMPL_H_
