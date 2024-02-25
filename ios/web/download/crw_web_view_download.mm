// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/crw_web_view_download.h"

#import <WebKit/WebKit.h>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"

@interface CRWWebViewDownload () <WKDownloadDelegate>

// Request URL called for the download.
@property(nonatomic, strong) NSURLRequest* request;

// Download object of a web resource.
@property(nonatomic, strong) WKDownload* download;

@end

@implementation CRWWebViewDownload {
  BOOL _isLocalDownload;
}

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
  if ([self.request.URL isFileURL]) {
    [self startLocalDownload];
    return;
  }
  [self.webView startDownloadUsingRequest:self.request
                        completionHandler:^(WKDownload* download) {
                          download.delegate = self;
                          self.download = download;
                        }];
}

- (void)cancelDownload:(ProceduralBlock)completion {
  if (_isLocalDownload) {
    if (completion) {
      completion();
    }
    return;
  }
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

#pragma mark - Private

// Start a `local download` which is really a copy from the request URL to the
// destination path.
- (void)startLocalDownload {
  _isLocalDownload = YES;
  __weak __typeof(self) weakSelf = self;
  base::FilePath sourcePath(base::SysNSStringToUTF8(self.request.URL.path));
  base::FilePath destPath(base::SysNSStringToUTF8(self.destinationPath));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&base::CopyFile, sourcePath, destPath),
      base::BindOnce(^(bool result) {
        if (result) {
          [weakSelf.delegate downloadDidFinish];
        } else {
          NSError* error = [NSError errorWithDomain:NSCocoaErrorDomain
                                               code:NSFileReadUnknownError
                                           userInfo:nil];
          [weakSelf.delegate downloadDidFailWithError:error];
        }
      }));
}

@end
