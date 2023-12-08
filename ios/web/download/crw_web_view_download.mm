// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/crw_web_view_download.h"

#import <WebKit/WebKit.h>

#import "base/ios/block_types.h"

@interface CRWWebViewDownload () <WKDownloadDelegate>

// Request URL called for the download.
@property(nonatomic, strong) NSURLRequest* request;

// Download object of a web resource.
@property(nonatomic, strong) WKDownload* download;

@end

@implementation CRWWebViewDownload

- (instancetype)initWithPath:(NSString*)destination
                     request:(NSURLRequest*)request
                     webview:(WKWebView*)webview
                    delegate:(id<CRWWebViewDownloadDelegate>)delegate {
  self = [super init];
  if (self) {
    self.destinationPath = destination;
    self.request = request;
    self.webView = webview;
    self.delegate = delegate;
  }
  return self;
}

- (void)startDownload {
  [self.webView startDownloadUsingRequest:self.request
                        completionHandler:^(WKDownload* download) {
                          download.delegate = self;
                          self.download = download;
                        }];
}

- (void)cancelDownload:(ProceduralBlock)completion {
  [self.download cancel:^(NSData* resumeData) {
    if (completion) {
      completion();
    }
  }];
}

#pragma mark - WKDownloadDelegate

- (void)download:(WKDownload*)download
    decideDestinationUsingResponse:(NSURLResponse*)response
                 suggestedFilename:(NSString*)suggestedFilename
                 completionHandler:
                     (void (^)(NSURL* destination))completionHandler {
  NSURL* destinationURL = [NSURL fileURLWithPath:self.destinationPath];
  completionHandler(destinationURL);
}

- (void)downloadDidFinish:(WKDownload*)download {
  [self.delegate downloadDidFinish];
}

- (void)download:(WKDownload*)download
    didFailWithError:(NSError*)error
          resumeData:(NSData*)resumeData {
  [self.delegate downloadDidFailWithError:error];
}

@end
