// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_native_task_bridge.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/files/file_util.h"
#import "base/functional/callback.h"
#import "base/task/thread_pool.h"
#import "ios/web/download/download_result.h"
#import "ios/web/web_view/error_translation_util.h"
#import "net/base/net_errors.h"

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

// Represents the possible state of the DownloadNativeTaskBridge.
enum class DownloadNativeTaskState {
  // The object has been initialized.
  kInitialized,

  // The download is in progress.
  kInProgress,

  // The download has been resumed. It is waiting for WebKit to acknowledge
  // the download request and ask for the path for the file.
  kResumed,

  // WebKit is ready to start the download. It is waiting for Chromium to
  // provide the path where the data should be written to disk.
  kPendingStart,

  // The download has been stopped (either cancelled, or due to an error)
  // and can be resumed (i.e. _resumeData is not nil).
  kStoppedResumable,

  // The download has been stopped (either cancelled, or due to an error)
  // but cannot be resume (i.e. _resumeData is nil).
  kStoppedPermanent,
};

}  // anonymous namespace

@implementation DownloadNativeTaskBridge {
  void (^_startDownloadBlock)(NSURL*);
  id<DownloadNativeTaskBridgeDelegate> _delegate;
  NativeDownloadTaskProgressCallback _progressCallback;
  NativeDownloadTaskResponseCallback _responseCallback;
  NativeDownloadTaskCompleteCallback _completeCallback;
  DownloadNativeTaskState _status;
  WKDownload* _download;
  NSData* _resumeData;
}

- (instancetype)initWithDownload:(WKDownload*)download
                        delegate:
                            (id<DownloadNativeTaskBridgeDelegate>)delegate {
  if ((self = [super init])) {
    _download = download;
    _delegate = delegate;
    _download.delegate = self;

    _status = DownloadNativeTaskState::kInitialized;
  }
  return self;
}

- (void)dealloc {
  [self stopObservingDownloadProgress];

  // At this point, _startDownloadBlock should be nil. However as seen in
  // https://crbug.com/344476170 it appears this invariant is not true.
  // Since WebKit terminates the app with a NSException if the block is
  // not called, invoke here if it is still set. This is a workaround
  // until a proper fix is implemented (i.e. using a real state object
  // to ensure that it is not possible for the block to not be invoked
  // when the `_status` changes).
  if (_startDownloadBlock) {
    _startDownloadBlock(nil);
    _startDownloadBlock = nil;
  }
}

- (void)cancel {
  if (_status == DownloadNativeTaskState::kPendingStart) {
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

    CHECK(_startDownloadBlock);
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
    [weakSelf stoppedWithResumeData:data];
  }];
  _download = nil;
}

- (void)startDownload:(const base::FilePath&)path
     progressCallback:(NativeDownloadTaskProgressCallback)progressCallback
     responseCallback:(NativeDownloadTaskResponseCallback)responseCallback
     completeCallback:(NativeDownloadTaskCompleteCallback)completeCallback {
  CHECK(!path.empty());

  _progressCallback = std::move(progressCallback);
  _responseCallback = std::move(responseCallback);
  _completeCallback = std::move(completeCallback);
  _urlForDownload = base::apple::FilePathToNSURL(path);

  switch (_status) {
    case DownloadNativeTaskState::kPendingStart: {
      [self responseReceived:_response];
      [self startObservingDownloadProgress];
      _startDownloadBlock(_urlForDownload);
      _startDownloadBlock = nil;
      return;
    }

    case DownloadNativeTaskState::kStoppedResumable: {
      CHECK(_resumeData);
      __weak __typeof(self) weakSelf = self;
      _status = DownloadNativeTaskState::kResumed;
      [_delegate resumeDownloadNativeTask:_resumeData
                        completionHandler:^(WKDownload* download) {
                          [weakSelf onResumedDownload:download];
                        }];
      return;
    }

    default: {
      [self downloadDidFailWithErrorCode:net::ERR_UNEXPECTED resumeData:nil];
      return;
    }
  }
}

- (void)stoppedWithResumeData:(NSData*)resumeData {
  _resumeData = resumeData;
  _status = _resumeData ? DownloadNativeTaskState::kStoppedResumable
                        : DownloadNativeTaskState::kStoppedPermanent;
}

- (void)onResumedDownload:(WKDownload*)download {
  _resumeData = nil;
  if (download) {
    _download = download;
    _download.delegate = self;
    // WKDownload will call
    //-decideDestinationUsingResponse:suggestedFilename:completionHandler:
    // where the download will be started.
  } else {
    _progressCallback.Reset();

    _status = DownloadNativeTaskState::kStoppedPermanent;
    web::DownloadResult download_result(net::ERR_FAILED, /*can_retry=*/false);
    std::move(_completeCallback).Run(download_result);
  }
}

- (void)downloadDidFailWithErrorCode:(int)errorCode
                          resumeData:(NSData*)resumeData {
  [self stopObservingDownloadProgress];
  [self stoppedWithResumeData:resumeData];
  if (!_completeCallback.is_null()) {
    _progressCallback.Reset();

    web::DownloadResult download_result(errorCode, resumeData != nil);
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
                 completionHandler:(void (^)(NSURL* destination))handler {
  CHECK_EQ(download, _download);

  _response = response;
  _suggestedFilename = suggestedFilename;
  [self responseReceived:_response];

  switch (_status) {
    case DownloadNativeTaskState::kInitialized: {
      CHECK(!_startDownloadBlock);
      _startDownloadBlock = handler;
      _status = DownloadNativeTaskState::kPendingStart;
      if (![_delegate onDownloadNativeTaskBridgeReadyForDownload:self]) {
        [self cancel];
      }
      return;
    }

    case DownloadNativeTaskState::kResumed: {
      CHECK(_urlForDownload);
      [self startObservingDownloadProgress];
      handler(_urlForDownload);
      return;
    }

    // Under certain circumstances, it was found that this method may be called
    // multiple times for the same object by WebKit. This may be due to a bug
    // in WebKit or in Chromium. Investigation is still pending.
    //
    // When this happen, the download cannot make progress if we call either of
    // the `handler` block passed, and if the UI is notified, the code will try
    // to cancel the download before starting it. To prevent entering this state
    // mark the download as in a permanent error.
    //
    // TODO(crbug.com/340644917): Explain root cause when found.
    default: {
      [self downloadDidFailWithErrorCode:net::ERR_UNEXPECTED resumeData:nil];
      if (_startDownloadBlock) {
        _startDownloadBlock(nil);
        _startDownloadBlock = nil;
      }
      handler(nil);
      return;
    }
  }
}

- (void)download:(WKDownload*)download
    didFailWithError:(NSError*)error
          resumeData:(NSData*)resumeData {
  int errorCode = net::OK;
  NSURL* url = _response.URL;
  if (!web::GetNetErrorFromIOSErrorCode(error.code, &errorCode, url)) {
    errorCode = net::ERR_FAILED;
  }

  [self downloadDidFailWithErrorCode:errorCode resumeData:resumeData];
}

- (void)downloadDidFinish:(WKDownload*)download {
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
        base::BindOnce(&FileSizeForFileAtPath,
                       base::apple::NSStringToFilePath(_urlForDownload.path)),
        base::BindOnce(&DownloadDidFinishWithSize, std::move(_progressCallback),
                       std::move(_completeCallback)));
  }
}

#pragma mark - KVO

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  CHECK_EQ(_status, DownloadNativeTaskState::kInProgress);
  if (!_progressCallback.is_null()) {
    NSProgress* progress = self.progress;
    _progressCallback.Run(progress.completedUnitCount, progress.totalUnitCount,
                          progress.fractionCompleted);
  }
}

#pragma mark - Private methods

- (void)startObservingDownloadProgress {
  CHECK_NE(_status, DownloadNativeTaskState::kInProgress);
  _status = DownloadNativeTaskState::kInProgress;
  [self.progress addObserver:self
                  forKeyPath:@"fractionCompleted"
                     options:NSKeyValueObservingOptionNew
                     context:nil];
}

- (void)stopObservingDownloadProgress {
  if (_status == DownloadNativeTaskState::kInProgress) {
    [self.progress removeObserver:self
                       forKeyPath:@"fractionCompleted"
                          context:nil];
  }
  _status = DownloadNativeTaskState::kStoppedPermanent;
}

- (void)responseReceived:(NSURLResponse*)response {
  if (_responseCallback.is_null()) {
    return;
  }

  int http_error = -1;
  if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
    http_error =
        base::apple::ObjCCastStrict<NSHTTPURLResponse>(response).statusCode;
  }

  std::move(_responseCallback).Run(http_error, response.MIMEType);
}

@end
