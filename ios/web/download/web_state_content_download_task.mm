// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/web_state_content_download_task.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/public/web_state.h"
#import "net/base/net_errors.h"

typedef void (^ProceduralBlockWithError)(NSError*);

@interface WebStateDownloadDelegateBridge
    : NSObject <CRWWebViewDownloadDelegate>
@end

@implementation WebStateDownloadDelegateBridge {
  base::OnceCallback<void(NSError*)> _callback;
}

- (instancetype)initWithCallback:(base::OnceCallback<void(NSError*)>)callback {
  self = [super init];
  if (self) {
    _callback = std::move(callback);
  }
  return self;
}
- (void)downloadDidFinish {
  std::move(_callback).Run(nil);
}

- (void)downloadDidFailWithError:(NSError*)error {
  std::move(_callback).Run(error);
}

@end

namespace web {
WebStateContentDownloadTask::WebStateContentDownloadTask(
    WebState* web_state,
    const GURL& original_url,
    NSString* http_method,
    const std::string& content_disposition,
    int64_t total_bytes,
    const std::string& mime_type,
    NSString* identifier,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : DownloadTaskImpl(web_state,
                       original_url,
                       http_method,
                       content_disposition,
                       total_bytes,
                       mime_type,
                       identifier,
                       task_runner) {}

WebStateContentDownloadTask::~WebStateContentDownloadTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelInternal();
}

void WebStateContentDownloadTask::StartInternal(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto callback =
      base::BindOnce(&WebStateContentDownloadTask::DownloadDidFinish,
                     weak_factory_.GetWeakPtr());
  download_delegate_ = [[WebStateDownloadDelegateBridge alloc]
      initWithCallback:std::move(callback)];
  web_state_->DownloadCurrentPage(
      base::SysUTF8ToNSString(path.value()), download_delegate_,
      base::CallbackToBlock(
          base::BindOnce(&WebStateContentDownloadTask::DownloadWasCreated,
                         weak_factory_.GetWeakPtr())));
}

void WebStateContentDownloadTask::CancelInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  // Client will receive status update through DownloadTask status update.
  [download_ cancelDownload:^{
  }];
}

#pragma mark - Private

void WebStateContentDownloadTask::DownloadWasCreated(
    id<CRWWebViewDownload> download) {
  download_ = download;
  // Task startup is complete, trigger observers.
  OnDownloadUpdated();
}

void WebStateContentDownloadTask::DownloadDidFinish(NSError* error) {
  if (!error) {
    percent_complete_ = 100;
    received_bytes_ = total_bytes_;
    OnDownloadFinished(DownloadResult(net::OK));
  } else {
    OnDownloadFinished(DownloadResult(net::ERR_FAILED));
  }
}

}  // namespace web
