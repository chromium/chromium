// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_DOWNLOAD_TASK_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_DOWNLOAD_TASK_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@protocol CWVDownloadTaskDelegate;

// Indicates that a file size is unknown.
FOUNDATION_EXPORT CWV_EXPORT int64_t const CWVDownloadSizeUnknown;

// The error domain for download errors.
FOUNDATION_EXPORT CWV_EXPORT NSErrorDomain const CWVDownloadErrorDomain;

// An error code for CWVDownloadErrorDomain for generic failure.
FOUNDATION_EXPORT CWV_EXPORT NSInteger const CWVDownloadErrorFailed;

// An error code for CWVDownloadErrorDomain when the task is aborted
// (cancelled).
FOUNDATION_EXPORT CWV_EXPORT NSInteger const CWVDownloadErrorAborted;

// Represents a single browser download task.
CWV_EXPORT
@interface CWVDownloadTask : NSObject

// Suggested name for the downloaded file including an extension.
@property(nonatomic, readonly) NSString* suggestedFileName;

// Effective MIME type of downloaded content.
@property(nonatomic, readonly) NSString* MIMEType;

// The URL that the download request originally attempted to fetch. This may
// differ from the final download URL if there were redirects.
@property(nonatomic, readonly) NSURL* originalURL;

// Total number of expected bytes (a best-guess upper-bound). Returns
// CWVDownloadSizeUnknown if the total size is unknown.
@property(nonatomic, readonly) int64_t totalBytes;

// Total number of bytes that have been received.
@property(nonatomic, readonly) int64_t receivedBytes;

// Rough progress of download, between 0.0 and 1.0.
// It is NAN when the progress is unknown.
@property(nonatomic, readonly) double progress;

@property(nonatomic, weak, nullable) id<CWVDownloadTaskDelegate> delegate;

- (instancetype)init NS_UNAVAILABLE;

// Starts to download the file to a local file at |path|.
//
// The local file is not deleted automatically. It is the caller's
// responsibility to delete it when it is unnecessary. This method can only be
// called if the task is not in progress.
//
// NOTE: It is currently required that an instance of CWVWebView which created
// this task is not deallocated before this method is called.
// TODO(crbug.com/40613954): Remove the restriction.
- (void)startDownloadToLocalFileAtPath:(NSString*)path;

// Cancels the download.
//
// It triggers a delegate method -downloadTask:didFinishWithError: with
// |error.code| CWVDownloadErrorAborted. Cancelled download can be restarted by
// calling -startDownloadToLocalFileAtPath:
- (void)cancel;

@end

// Delegate to observe updates to CWVDownloadTask.
@protocol CWVDownloadTaskDelegate<NSObject>
@optional

// Called when the download has finished. |error| is nil when it has completed
// successfully. |error| represents the error when the download has failed
// (e.g., due to network errors) or has been cancelled. |error.code| is either
// CWVDownloadErrorFailed or CWVDownloadErrorAborted. |error| also contains a
// description which describes the type of an error.
- (void)downloadTask:(CWVDownloadTask*)downloadTask
    didFinishWithError:(nullable NSError*)error;

// Called when the progress of the download has changed. Refer to task.progress
// to check the progress.
- (void)downloadTaskProgressDidChange:(CWVDownloadTask*)downloadTask;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_DOWNLOAD_TASK_H_
