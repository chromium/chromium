// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_SAFARI_DOWNLOAD_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_SAFARI_DOWNLOAD_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate for SafariDownloadTabHelper class.
@protocol SafariDownloadTabHelperDelegate

// Called to download .mobileconfig file, `fileURL` points to the .mobileconfig
// file that we are trying to download. If `fileURL` is nil then this is no-op.
- (void)presentMobileConfigAlertFromURL:(NSURL*)fileURL;

// Called to download .ics file, `fileURL` points to the .ics
// file that we are trying to download. If `fileURL` is nil then this is no-op.
- (void)presentCalendarAlertFromURL:(NSURL*)fileURL;

// Called to download .order files, `fileURL` points to the .order file that we
// are trying to download. If `fileURL` is nil then this is no-op.
- (void)presentAppleWalletOrderAlertFromURL:(NSURL*)fileURL;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_SAFARI_DOWNLOAD_TAB_HELPER_DELEGATE_H_
