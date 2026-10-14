// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/crw_web_view_download.h"

#import <WebKit/WebKit.h>

#import <optional>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/time/time.h"
#import "base/timer/elapsed_timer.h"

@interface CRWWebViewDownload () <WKDownloadDelegate>

// Request URL called for the download.
@property(nonatomic, strong) NSURLRequest* request;

// Download object of a web resource.
@property(nonatomic, strong) WKDownload* download;

@end

@implementation CRWWebViewDownload {
  BOOL _isLocalDownload;
  std::optional<base::ElapsedTimer> _downloadTimer;
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
  _downloadTimer.emplace();
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
  __weak __typeof(self) weakSelf = self;
  [self.download cancel:^(NSData* resumeData) {
    [weakSelf recordResult:web::IOSDownloadWebViewResult::kCanceled];
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
  [self recordResult:web::IOSDownloadWebViewResult::kCompleted];
  [self.delegate downloadDidFinish];
}

- (void)download:(WKDownload*)download
    didFailWithError:(NSError*)error
          resumeData:(NSData*)resumeData {
  [self recordResult:web::IOSDownloadWebViewResult::kError];
  [self.delegate downloadDidFailWithError:error];
}

#pragma mark - Private

// Records metrics for the `WKDownload` outcome.
- (void)recordResult:(web::IOSDownloadWebViewResult)result {
  // `_downloadTimer` is reset after the first report to ensure at-most-once
  // recording (e.g. if `cancelDownload:` is called during teardown after
  // completion or failure).
  if (!_downloadTimer.has_value()) {
    return;
  }
  base::UmaHistogramEnumeration(web::kIOSDownloadWebViewResultHistogram,
                                result);
  if (result == web::IOSDownloadWebViewResult::kCompleted) {
    base::UmaHistogramMediumTimes(web::kIOSDownloadWebViewDurationHistogram,
                                  _downloadTimer->Elapsed());
  }
  _downloadTimer.reset();
}

// Starts a local download by copying the file from the request URL to the
// destination path on a background thread.
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
        [weakSelf localDownloadFinishedWithSuccess:result];
      }));
}

// Logs metrics and notifies the delegate when a local download finishes.
- (void)localDownloadFinishedWithSuccess:(BOOL)success {
  base::UmaHistogramBoolean(web::kIOSDownloadWebViewLocalSucceededHistogram,
                            success);
  if (success) {
    [self.delegate downloadDidFinish];
    return;
  }
  NSError* error = [NSError errorWithDomain:NSCocoaErrorDomain
                                       code:NSFileReadUnknownError
                                   userInfo:nil];
  [self.delegate downloadDidFailWithError:error];
}

@end
