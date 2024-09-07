// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/fake_native_task_bridge.h"

#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "net/base/net_errors.h"

@implementation FakeNativeTaskBridge {
  void (^_startDownloadBlock)(NSURL*);
  BOOL _observingDownloadProgress;
  NativeDownloadTaskCompleteCallback _completeCallback;
}

@synthesize download = _download;
@synthesize progress = _progress;
@synthesize response = _response;
@synthesize suggestedFilename = _suggestedFilename;

- (instancetype)initWithDownload:(WKDownload*)download
                        delegate:
                            (id<DownloadNativeTaskBridgeDelegate>)delegate {
  if ((self = [super initWithDownload:download delegate:delegate])) {
    _calledStartDownloadBlock = NO;
    [self downloadInitialized];
  }
  return self;
}

- (void)cancel {
  if (_startDownloadBlock) {
    _startDownloadBlock(nil);
  }
  _progress = [NSProgress progressWithTotalUnitCount:0];
}

- (void)startDownload:(const base::FilePath&)path
     progressCallback:(NativeDownloadTaskProgressCallback)progressCallback
     responseCallback:(NativeDownloadTaskResponseCallback)responseCallback
     completeCallback:(NativeDownloadTaskCompleteCallback)completeCallback {
  _completeCallback = std::move(completeCallback);
  [self startObservingDownloadProgress];
  _startDownloadBlock(self.urlForDownload);
  _startDownloadBlock = nil;

  // Simulates completing a download progress
  _progress = [NSProgress progressWithTotalUnitCount:100];
}

- (void)dealloc {
  [self stopObservingDownloadProgress];
}

#pragma mark - Private methods

- (void)downloadInitialized {
  // Instantiates _startDownloadBlock, so when we call
  // startDownload:progressionHandler:completionHandler method, the block is
  // initialized.
  __weak FakeNativeTaskBridge* weakSelf = self;
  void (^handler)(NSURL* destination) = ^void(NSURL* url) {
    [weakSelf destinationDecided:url];
  };

  if (self.urlForDownload) {
    // Resuming a download.
    [self startObservingDownloadProgress];
    handler(self.urlForDownload);
  } else {
    _startDownloadBlock = handler;
  }
}

- (void)destinationDecided:(NSURL*)url {
  _calledStartDownloadBlock = YES;
  [self downloadDidFinish:_download];
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

- (void)downloadDidFinish:(WKDownload*)download {
  [self stopObservingDownloadProgress];
  if (!_completeCallback.is_null()) {
    web::DownloadResult download_result(net::OK);
    std::move(_completeCallback).Run(download_result);
  }
}

@end
