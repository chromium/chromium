// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_MANAGER_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_MANAGER_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

class DownloadManagerTabHelper;
namespace web {
class DownloadTask;
}  // namespace web

// Whether or not the new download task should replace the old download task.
typedef NS_ENUM(NSInteger, NewDownloadPolicy) {
  // Old download task should be discarded and replaced with a new download
  // task.
  kNewDownloadPolicyReplace = 0,
  // Old download task should be kept. A new download task should be discarded.
  kNewDownloadPolicyDiscard,
};

// Delegate for DownloadManagerTabHelper class.
@protocol DownloadManagerTabHelperDelegate<NSObject>

// Informs the delegate that a DownloadTask was created.
- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
               didCreateDownload:(nonnull web::DownloadTask*)download
               webStateIsVisible:(BOOL)webStateIsVisible;

// Asks the delegate whether the new download task should replace the old
// download task.
- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
         decidePolicyForDownload:(nonnull web::DownloadTask*)download
               completionHandler:(nonnull void (^)(NewDownloadPolicy))handler;

// Informs the delegate that WebState related to this download was hidden.
- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
                 didHideDownload:(nonnull web::DownloadTask*)download;

// Informs the delegate that WebState related to this download was shown.
- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
                 didShowDownload:(nonnull web::DownloadTask*)download;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_MANAGER_TAB_HELPER_DELEGATE_H_
