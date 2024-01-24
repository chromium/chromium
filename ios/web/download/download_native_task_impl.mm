// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_native_task_impl.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/web/download/download_native_task_bridge.h"

namespace web {

DownloadNativeTaskImpl::DownloadNativeTaskImpl(
    WebState* web_state,
    const GURL& original_url,
    NSString* http_method,
    const std::string& content_disposition,
    int64_t total_bytes,
    const std::string& mime_type,
    NSString* identifier,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    DownloadNativeTaskBridge* download)
    : DownloadTaskImpl(web_state,
                       original_url,
                       http_method,
                       content_disposition,
                       total_bytes,
                       mime_type,
                       identifier,
                       task_runner),
      download_bridge_(download) {
  DCHECK(download_bridge_);
}

DownloadNativeTaskImpl::~DownloadNativeTaskImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelInternal();
}

void DownloadNativeTaskImpl::StartInternal(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!path.empty());
  DCHECK(download_bridge_);

  NativeDownloadTaskProgressCallback progress_callback = base::BindRepeating(
      &DownloadNativeTaskImpl::OnDownloadProgress, weak_factory_.GetWeakPtr());

  NativeDownloadTaskResponseCallback response_callback = base::BindOnce(
      &DownloadNativeTaskImpl::OnResponseReceived, weak_factory_.GetWeakPtr());

  NativeDownloadTaskCompleteCallback complete_callback = base::BindOnce(
      &DownloadNativeTaskImpl::OnDownloadFinished, weak_factory_.GetWeakPtr());

  [download_bridge_ startDownload:path
                 progressCallback:std::move(progress_callback)
                 responseCallback:std::move(response_callback)
                 completeCallback:std::move(complete_callback)];
}

void DownloadNativeTaskImpl::CancelInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  [download_bridge_ cancel];
  download_bridge_ = nil;
}

std::string DownloadNativeTaskImpl::GetSuggestedName() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::SysNSStringToUTF8(download_bridge_.suggestedFilename);
}

void DownloadNativeTaskImpl::OnDownloadProgress(int64_t bytes_received,
                                                int64_t total_bytes,
                                                double fraction_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  total_bytes_ = total_bytes;
  received_bytes_ = bytes_received;
  percent_complete_ = static_cast<int>(fraction_complete * 100);
  OnDownloadUpdated();
}

void DownloadNativeTaskImpl::OnResponseReceived(int http_error_code,
                                                NSString* mime_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  http_code_ = http_error_code;
  if (mime_type.length) {
    mime_type_ = base::SysNSStringToUTF8(mime_type);
  }
}

}  // namespace web
