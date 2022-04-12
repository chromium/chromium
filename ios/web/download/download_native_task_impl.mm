// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_native_task_impl.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/web/download/download_native_task_bridge.h"
#import "net/base/filename_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

DownloadNativeTaskImpl::DownloadNativeTaskImpl(
    WebState* web_state,
    const GURL& original_url,
    NSString* http_method,
    const std::string& content_disposition,
    int64_t total_bytes,
    const std::string& mime_type,
    NSString* identifier,
    DownloadNativeTaskBridge* download,
    Delegate* delegate)
    : DownloadTaskImpl(web_state,
                       original_url,
                       http_method,
                       content_disposition,
                       total_bytes,
                       mime_type,
                       identifier,
                       delegate),
      download_bridge_(download) {
  DCHECK(download_bridge_);
}

DownloadNativeTaskImpl::~DownloadNativeTaskImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (@available(iOS 15, *)) {
    [download_bridge_ cancel];
    download_bridge_ = nil;
  }
}

void DownloadNativeTaskImpl::Start(const base::FilePath& path,
                                   Destination destination_hint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DownloadTaskImpl::Start(path, destination_hint);
  download_path_ = path;
  // WKDownload can only download to a file. If the user has
  // not specified a destination path, save the file to the
  // suggested file name in a temporary directory.
  if (download_path_.empty()) {
    NSString* temporary_directory = NSTemporaryDirectory();
    NSString* temporary_filename = [temporary_directory
        stringByAppendingPathComponent:base::SysUTF16ToNSString(
                                           GetSuggestedFilename())];
    download_path_ =
        base::FilePath(base::SysNSStringToUTF8(temporary_filename));
  }

  if (@available(iOS 15, *)) {
    DCHECK(download_bridge_);
    NSURL* downloadURL = [NSURL
        fileURLWithPath:base::SysUTF8ToNSString(download_path_.AsUTF8Unsafe())];

    base::WeakPtr<DownloadNativeTaskImpl> weak_this =
        weak_factory_.GetWeakPtr();
    [download_bridge_ startDownload:downloadURL
        progressionHandler:^() {
          DownloadNativeTaskImpl* task = weak_this.get();
          if (task)
            task->OnDownloadUpdated();
        }
        completionHandler:^(DownloadResult download_result) {
          DownloadNativeTaskImpl* task = weak_this.get();
          if (task)
            task->OnDownloadFinished(download_result);
        }];
  }
}

void DownloadNativeTaskImpl::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (@available(iOS 15, *)) {
    [download_bridge_ cancel];
    download_bridge_ = nil;
  }
  DownloadTaskImpl::Cancel();
}

void DownloadNativeTaskImpl::ShutDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (@available(iOS 15, *)) {
    [download_bridge_ cancel];
    download_bridge_ = nil;
  }
  DownloadTaskImpl::ShutDown();
}

NSData* DownloadNativeTaskImpl::GetResponseData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (@available(iOS 15, *)) {
    return [NSData dataWithContentsOfURL:[download_bridge_ urlForDownload]];
  }
  return nil;
}

const base::FilePath& DownloadNativeTaskImpl::GetResponsePath() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (@available(iOS 15, *)) {
    return download_path_;
  }
  static const base::FilePath kEmptyPath;
  return kEmptyPath;
}

int64_t DownloadNativeTaskImpl::GetTotalBytes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (@available(iOS 15, *)) {
    return download_bridge_.progress.totalUnitCount;
  }
  return total_bytes_;
}

int64_t DownloadNativeTaskImpl::GetReceivedBytes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (@available(iOS 15, *)) {
    return download_bridge_.progress.completedUnitCount;
  }
  return received_bytes_;
}

int DownloadNativeTaskImpl::GetPercentComplete() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (@available(iOS 15, *)) {
    return static_cast<int>(download_bridge_.progress.fractionCompleted * 100);
  }
  return percent_complete_;
}

std::u16string DownloadNativeTaskImpl::GetSuggestedFilename() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string suggested_filename;
  if (@available(iOS 15, *)) {
    suggested_filename =
        base::SysNSStringToUTF8(download_bridge_.suggestedFilename);
  }
  return net::GetSuggestedFilename(GetOriginalUrl(), GetContentDisposition(),
                                   /*referrer_charset=*/std::string(),
                                   /*suggested_name=*/suggested_filename,
                                   /*mime_type=*/std::string(),
                                   /*default_name=*/"document");
}

}  // namespace web
