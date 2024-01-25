// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DOWNLOAD_CRW_WEB_VIEW_DOWNLOAD_H_
#define IOS_WEB_PUBLIC_DOWNLOAD_CRW_WEB_VIEW_DOWNLOAD_H_

#import <Foundation/Foundation.h>
#import "base/ios/block_types.h"

// Provides API for managing a web view download.
@protocol CRWWebViewDownload <NSObject>

// Cancels the download with a completion block.
// Local downloads (from file:// URL) cannot be cancelled.
- (void)cancelDownload:(ProceduralBlock)completion;

@end

// Delegate for CRWWebViewDownload.
@protocol CRWWebViewDownloadDelegate

// Notifies the delegate that the download has finished.
- (void)downloadDidFinish;

// Notifies the delegate that the download failed, with error information.
- (void)downloadDidFailWithError:(NSError*)error;

@end

#endif  // IOS_WEB_PUBLIC_DOWNLOAD_CRW_WEB_VIEW_DOWNLOAD_H_
