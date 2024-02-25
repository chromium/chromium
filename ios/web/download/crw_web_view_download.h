// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_CRW_WEB_VIEW_DOWNLOAD_H_
#define IOS_WEB_DOWNLOAD_CRW_WEB_VIEW_DOWNLOAD_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/download/crw_web_view_download.h"

@class WKWebView;

@interface CRWWebViewDownload : NSObject <CRWWebViewDownload>

// Destination path where the file is saved.
@property(nonatomic, strong) NSString* destinationPath;

// Web view used to call the download.
@property(nonatomic, strong) WKWebView* webView;

// CRWWebViewDownloadDelegate used to track download status.
@property(nonatomic, weak) id<CRWWebViewDownloadDelegate> delegate;

// Initializes CRWWebViewDownload.
// `destination` is the destination path where the file is save.
// `request` is the request called for the download.
// `webview` is the web view used to call the download.
// `delegate` is the delegate used to track download status.
- (instancetype)initWithPath:(NSString*)destination
                     request:(NSURLRequest*)request
                     webview:(WKWebView*)webview
                    delegate:(id<CRWWebViewDownloadDelegate>)delegate;

// Starts to download the resource at the URL in the request specified at the
// initialization.
- (void)startDownload;

@end

#endif  // IOS_WEB_DOWNLOAD_CRW_WEB_VIEW_DOWNLOAD_H_
