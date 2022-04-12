// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DOWNLOAD_NATIVE_TASK_IMPL_H_
#define IOS_WEB_DOWNLOAD_DOWNLOAD_NATIVE_TASK_IMPL_H_

#import <WebKit/WebKit.h>

#include "base/sequence_checker.h"
#include "ios/web/download/download_task_impl.h"

@class DownloadNativeTaskBridge;

namespace web {

// Implementation of DownloadTaskImpl that uses WKDownload (wrapped in
// NativeTaskBridge) to perform the download
class DownloadNativeTaskImpl final : public DownloadTaskImpl {
 public:
  // Constructs a new DownloadSessionTaskImpl objects. |web_state|, |identifier|
  // |delegate|, and |download| must be valid.
  DownloadNativeTaskImpl(WebState* web_state,
                         const GURL& original_url,
                         NSString* http_method,
                         const std::string& content_disposition,
                         int64_t total_bytes,
                         const std::string& mime_type,
                         NSString* identifier,
                         DownloadNativeTaskBridge* download,
                         Delegate* delegate) API_AVAILABLE(ios(15));

  DownloadNativeTaskImpl(const DownloadNativeTaskImpl&) = delete;
  DownloadNativeTaskImpl& operator=(const DownloadNativeTaskImpl&) = delete;

  ~DownloadNativeTaskImpl() final;

  // DownloadTaskImpl overrides:
  void Start(const base::FilePath& path, Destination destination_hint) final;
  void Cancel() final;
  void ShutDown() final;

  // DownloadTask overrides:
  NSData* GetResponseData() const final;
  const base::FilePath& GetResponsePath() const final;
  int64_t GetTotalBytes() const final;
  int64_t GetReceivedBytes() const final;
  int GetPercentComplete() const final;
  std::u16string GetSuggestedFilename() const final;

 private:
  DownloadNativeTaskBridge* download_bridge_ API_AVAILABLE(ios(15)) = nil;
  base::FilePath download_path_;

  base::WeakPtrFactory<DownloadNativeTaskImpl> weak_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_DOWNLOAD_DOWNLOAD_NATIVE_TASK_IMPL_H_
