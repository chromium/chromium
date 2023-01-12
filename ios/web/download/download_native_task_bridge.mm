// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_native_task_bridge.h"

#import "base/check.h"
#import "base/files/file_util.h"
#import "base/functional/callback.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "ios/web/download/download_result.h"
#import "ios/web/web_view/error_translation_util.h"
#import "net/base/net_errors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Helper to get the size of file at `file_path`. Returns -1 in case of error.
int64_t FileSizeForFileAtPath(base::FilePath file_path) {
  int64_t file_size = 0;
  if (!base::GetFileSize(file_path, &file_size))
    return -1;

  return file_size;
}

// Helper to invoke the download complete callback after getting the file
// size.
void DownloadDidFinishWithSize(
    NativeDownloadTaskProgressCallback progress_callback,
    NativeDownloadTaskCompleteCallback complete_callback,
    int64_t file_size) {
  if (file_size != -1 && !progress_callback.is_null()) {
    progress_callback.Run(
        /* bytes_received */ file_size, /* total_bytes */ file_size,
        /* fraction_completed */ 1.0);
  }

  web::DownloadResult download_result(net::OK);
  std::move(complete_callback).Run(download_result);
}

}  // anonymous namespace

@interface DownloadNativeTaskBridge ()

@property(nonatomic, readwrite, strong) NSData* resumeData;
@property(nonatomic, readwrite, strong)
    WKDownload* download API_AVAILABLE(ios(15));

@end

@implementation DownloadNativeTaskBridge {
  void (^_startDownloadBlock)(NSURL*);
  id<DownloadNativeTaskBridgeDelegate> _delegate;
  NativeDownloadTaskProgressCallback _progressCallback;
  NativeDownloadTaskResponseCallback _responseCallback;
  NativeDownloadTaskCompleteCallback _completeCallback;
  BOOL _observingDownloadProgress;
}

- (instancetype)initWithDownload:(WKDownload*)download
                        delegate:(id<DownloadNativeTaskBridgeDelegate>)delegate
    API_AVAILABLE(ios(15)) {
  if ((self = [super init])) {
    _download = download;
    _delegate = delegate;
    _download.delegate = self;
  }
  return self;
}

- (void)dealloc {
  [self stopObservingDownloadProgress];
}

- (void)cancel {
  if (_startDownloadBlock) {
    // WKDownload will pass a block to its delegate when calling its
    // - download:decideDestinationUsingResponse:suggestedFilename
    //:completionHandler: method. WKDownload enforces that this block is called
    // before the object is destroyed or the download is cancelled. Thus it
    // must be called now.
    //
    // Call it with a temporary path, and schedule a block to delete the file
    // later (to avoid keeping the file around). Use a random non-empty name
    // for the file as `self.suggestedFilename` can be `nil` which would result
    // in the deletion of the directory `NSTemporaryDirectory()` preventing the
    // creation of any temporary file afterwards.
    NSString* filename = [[NSUUID UUID] UUIDString];
    NSURL* url =
        [NSURL fileURLWithPath:[NSTemporaryDirectory()
                                   stringByAppendingPathComponent:filename]];

    _startDownloadBlock(url);
    _startDownloadBlock = nil;

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0), ^{
      NSFileManager* manager = [NSFileManager defaultManager];
      [manager removeItemAtURL:url error:nil];
    });
  }

  [self stopObservingDownloadProgress];

  __weak __typeof(self) weakSelf = self;
  [_download cancel:^(NSData* data) {
    weakSelf.resumeData = data;
  }];
  _download = nil;
}

- (void)startDownload:(const base::FilePath&)path
     progressCallback:(NativeDownloadTaskProgressCallback)progressCallback
     responseCallback:(NativeDownloadTaskResponseCallback)responseCallback
     completeCallback:(NativeDownloadTaskCompleteCallback)completeCallback {
  DCHECK(!path.empty());

  _progressCallback = std::move(progressCallback);
  _responseCallback = std::move(responseCallback);
  _completeCallback = std::move(completeCallback);
  _urlForDownload =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.AsUTF8Unsafe())];

  if (_resumeData) {
    DCHECK(!_startDownloadBlock);
    if (@available(iOS 15, *)) {
      __weak __typeof(self) weakSelf = self;
      [_delegate resumeDownloadNativeTask:_resumeData
                        completionHandler:^(WKDownload* download) {
                          [weakSelf onResumedDownload:download];
                        }];
    }
    return;
  }

  [self responseReceived:_response];
  [self startObservingDownloadProgress];
  _startDownloadBlock(_urlForDownload);
  _startDownloadBlock = nil;
}

- (void)onResumedDownload:(WKDownload*)download API_AVAILABLE(ios(15)) {
  _resumeData = nil;
  if (download) {
    _download = download;
    _download.delegate = self;
    // WKDownload will call
    //-decideDestinationUsingResponse:suggestedFilename:completionHandler:
    // where the download will be started.
  } else {
    _progressCallback.Reset();

    web::DownloadResult download_result(net::ERR_FAILED, /*can_retry=*/false);
    std::move(_completeCallback).Run(download_result);
  }
}

#pragma mark - Properties

- (NSProgress*)progress {
  return _download.progress;
}

#pragma mark - WKDownloadDelegate

- (void)download:(WKDownload*)download
    decideDestinationUsingResponse:(NSURLResponse*)response
                 suggestedFilename:(NSString*)suggestedFilename
                 completionHandler:(void (^)(NSURL* destination))handler
    API_AVAILABLE(ios(15)) {
  _response = response;
  _suggestedFilename = suggestedFilename;
  [self responseReceived:_response];

  if (_urlForDownload) {
    // Resuming a download.
    [self startObservingDownloadProgress];
    handler(_urlForDownload);
  } else {
    _startDownloadBlock = handler;
    if (![_delegate onDownloadNativeTaskBridgeReadyForDownload:self]) {
      [self cancel];
    }
  }
}

- (void)download:(WKDownload*)download
    didFailWithError:(NSError*)error
          resumeData:(NSData*)resumeData API_AVAILABLE(ios(15)) {
  self.resumeData = resumeData;
  [self stopObservingDownloadProgress];
  if (!_completeCallback.is_null()) {
    _progressCallback.Reset();

    int error_code = net::OK;
    NSURL* url = _response.URL;
    if (!web::GetNetErrorFromIOSErrorCode(error.code, &error_code, url)) {
      error_code = net::ERR_FAILED;
    }

    web::DownloadResult download_result(error_code, resumeData != nil);
    std::move(_completeCallback).Run(download_result);
  }
}

- (void)downloadDidFinish:(WKDownload*)download API_AVAILABLE(ios(15)) {
  [self stopObservingDownloadProgress];
  if (!_completeCallback.is_null()) {
    // The method -downloadDidFinish: will be called as soon as the
    // download completes, even before the NSProgress item may have
    // been updated.
    //
    // To prevent truncating the downloaded file, get the real size
    // of the file from the disk and call `_progressCallback` first
    // before calling `_completeCallback`.
    //
    // See https://crbug.com/1346030 for examples of truncation.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(
            &FileSizeForFileAtPath,
            base::FilePath(base::SysNSStringToUTF8(_urlForDownload.path))),
        base::BindOnce(&DownloadDidFinishWithSize, std::move(_progressCallback),
                       std::move(_completeCallback)));
  }
}

#pragma mark - KVO

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context API_AVAILABLE(ios(15)) {
  if (!_progressCallback.is_null()) {
    NSProgress* progress = self.progress;
    _progressCallback.Run(progress.completedUnitCount, progress.totalUnitCount,
                          progress.fractionCompleted);
  }
}

#pragma mark - Private methods

- (void)startObservingDownloadProgress {
  DCHECK(!_observingDownloadProgress);

  _observingDownloadProgress = YES;
  [self.progress addObserver:self
                  forKeyPath:@"fractionCompleted"
                     options:NSKeyValueObservingOptionNew
                     context:nil];
}

- (void)stopObservingDownloadProgress {
  if (_observingDownloadProgress) {
    _observingDownloadProgress = NO;
    [self.progress removeObserver:self
                       forKeyPath:@"fractionCompleted"
                          context:nil];
  }
}

- (void)responseReceived:(NSURLResponse*)response {
  if (_responseCallback.is_null()) {
    return;
  }

  int http_error = -1;
  if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
    http_error =
        base::mac::ObjCCastStrict<NSHTTPURLResponse>(response).statusCode;
  }

  std::move(_responseCallback).Run(http_error, response.MIMEType);
}

@end
