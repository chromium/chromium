// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/fake_native_task_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeNativeTaskBridge

@synthesize download = _download;
@synthesize progress = _progress;
@synthesize response = _response;
@synthesize suggestedFilename = _suggestedFilename;

- (instancetype)initWithDownload:(WKDownload*)download
                        delegate:
                            (id<DownloadNativeTaskBridgeDelegate>)delegate {
  if (self = [super initWithDownload:download delegate:delegate]) {
    _calledStartDownloadBlock = NO;
    if (@available(iOS 15, *))
      [self downloadInitialized];
  }
  return self;
}

- (void)cancel {
  [super cancel];
  _progress = [NSProgress progressWithTotalUnitCount:0];
}

- (void)startDownload:(NSURL*)url
    progressionHandler:(void (^)())progressionHandler
     completionHandler:(web::DownloadCompletionHandler)completionHandler {
  [super startDownload:url
      progressionHandler:progressionHandler
       completionHandler:completionHandler];

  // Simulates completing a download progress
  _progress = [NSProgress progressWithTotalUnitCount:100];
}

- (void)downloadInitialized {
  // Instantiates _startDownloadBlock, so when we call
  // startDownload:progressionHandler:completionHandler method, the block is
  // initialized.
  [super download:_download
      decideDestinationUsingResponse:_response
                   suggestedFilename:_suggestedFilename
                   completionHandler:^void(NSURL* url) {
                     self->_calledStartDownloadBlock = YES;
                   }];
}

@end
