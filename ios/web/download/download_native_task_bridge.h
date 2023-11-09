// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DOWNLOAD_NATIVE_TASK_BRIDGE_H_
#define IOS_WEB_DOWNLOAD_DOWNLOAD_NATIVE_TASK_BRIDGE_H_

#import <WebKit/WebKit.h>

#include "base/functional/callback_forward.h"
#include "ios/web/download/download_result.h"

namespace base {
class FilePath;
}

// Callback invoked repeatedly when new data is received from the WKDownload*.
using NativeDownloadTaskProgressCallback =
    base::RepeatingCallback<void(int64_t bytes_received,
                                 int64_t total_bytes,
                                 double fraction_completed)>;

// Callback invoked once the NSURLResponse is received for the WKDownload*.
using NativeDownloadTaskResponseCallback =
    base::OnceCallback<void(int http_error_code, NSString* mime_type)>;

// Callback invoked once the WKDownload completes, possibly in error.
using NativeDownloadTaskCompleteCallback =
    base::OnceCallback<void(web::DownloadResult result)>;

@class DownloadNativeTaskBridge;

@protocol DownloadNativeTaskBridgeDelegate <NSObject>

// Used to set response url, content length, mimetype and http response headers
// in CRWWkNavigationHandler so method can interact with WKWebView.
- (BOOL)onDownloadNativeTaskBridgeReadyForDownload:
    (DownloadNativeTaskBridge*)bridge;

// Calls CRWWKNavigationHandlerDelegate to resume download using the web view.
- (void)resumeDownloadNativeTask:(NSData*)data
               completionHandler:(void (^)(WKDownload*))completionHandler;

@end

// Class used to create a download task object that handles downloads through
// WKDownload.
@interface DownloadNativeTaskBridge : NSObject <WKDownloadDelegate>

// Default initializer. `download` and `delegate` must be non-nil.
- (instancetype)initWithDownload:(WKDownload*)download
                        delegate:(id<DownloadNativeTaskBridgeDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Cancels download
- (void)cancel;

// Starts download to `path` with given `progressCallback`, `responseCallback`
// and `completeCallback`.
- (void)startDownload:(const base::FilePath&)path
     progressCallback:(NativeDownloadTaskProgressCallback)progressCallback
     responseCallback:(NativeDownloadTaskResponseCallback)responseCallback
     completeCallback:(NativeDownloadTaskCompleteCallback)completeCallback;

@property(nonatomic, readonly) WKDownload* download;
@property(nonatomic, readonly) NSURLResponse* response;
@property(nonatomic, readonly) NSString* suggestedFilename;
@property(nonatomic, readonly) NSProgress* progress;
@property(nonatomic, readonly) NSURL* urlForDownload;

@end

#endif  // IOS_WEB_DOWNLOAD_DOWNLOAD_NATIVE_TASK_BRIDGE_H_
