// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_native_task_bridge.h"

#import "base/check.h"
#include "ios/web/download/download_result.h"
#include "net/base/net_errors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation DownloadNativeTaskBridge {
  void (^_startDownloadBlock)(NSURL*);
  id<DownloadNativeTaskBridgeDelegate> _delegate;
  void (^_progressionHandler)();
  web::DownloadCompletionHandler _completionHandler;
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

  [_download cancel:^(NSData* data){/* do nothing */}];
  _download = nil;
}

- (void)startDownload:(NSURL*)url
    progressionHandler:(void (^)())progressionHandler
     completionHandler:(web::DownloadCompletionHandler)completionHandler {
  _progressionHandler = progressionHandler;
  _completionHandler = completionHandler;

  [self startObservingDownloadProgress];

  _urlForDownload = [url copy];

  _startDownloadBlock(_urlForDownload);
  _startDownloadBlock = nil;
}

#pragma mark - Properties

- (NSProgress*)progress {
  return _download.progress;
}

#pragma mark - WKDownloadDelegate

- (void)download:(WKDownload*)download
    decideDestinationUsingResponse:(NSURLResponse*)response
                 suggestedFilename:(NSString*)suggestedFilename
                 completionHandler:
                     (void (^)(NSURL* destination))completionHandler API_AVAILABLE(ios(15)) {
  _response = response;
  _suggestedFilename = suggestedFilename;
  _startDownloadBlock = completionHandler;

  [_delegate onDownloadNativeTaskBridgeReadyForDownload:self];
}

- (void)download:(WKDownload*)download
    didFailWithError:(NSError*)error
          resumeData:(NSData*)resumeData API_AVAILABLE(ios(15)) {
  if (_completionHandler) {
    web::DownloadResult download_result(net::ERR_FAILED, resumeData != nil);
    (_completionHandler)(download_result);
  }

  [self stopObservingDownloadProgress];
}

- (void)downloadDidFinish:(WKDownload*)download API_AVAILABLE(ios(15)) {
  if (_completionHandler) {
    web::DownloadResult download_result(net::OK);
    (_completionHandler)(download_result);
  }

  [self stopObservingDownloadProgress];
}

#pragma mark - KVO

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context API_AVAILABLE(ios(15)) {
  if (_progressionHandler)
    _progressionHandler();
}

#pragma mark - Private methods

- (void)startObservingDownloadProgress {
  DCHECK(!_observingDownloadProgress);

  _observingDownloadProgress = YES;
  [_download.progress addObserver:self
                       forKeyPath:@"fractionCompleted"
                          options:NSKeyValueObservingOptionNew
                          context:nil];
}

- (void)stopObservingDownloadProgress {
  if (_observingDownloadProgress) {
    _observingDownloadProgress = NO;
    [_download.progress removeObserver:self
                            forKeyPath:@"fractionCompleted"
                               context:nil];
  }
}

@end
