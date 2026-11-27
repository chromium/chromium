// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_SAFARI_DOWNLOAD_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_SAFARI_DOWNLOAD_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate for SafariDownloadTabHelper class.
@protocol SafariDownloadTabHelperDelegate

// Called to download a .mobileconfig file, where `fileURL` points to the file
// to download. No-op if `fileURL` is nil or not a valid HTTP(S) URL with a
// host.
- (void)presentMobileConfigAlertFromURL:(NSURL*)fileURL;

// Called to download an .ics file, where `fileURL` points to the file to
// download. No-op if `fileURL` is nil or not a valid HTTP(S) URL with a host.
- (void)presentCalendarAlertFromURL:(NSURL*)fileURL;

// Called to download an .order file, where `fileURL` points to the file to
// download. No-op if `fileURL` is nil or not a valid HTTP(S) URL with a host.
- (void)presentAppleWalletOrderAlertFromURL:(NSURL*)fileURL;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_SAFARI_DOWNLOAD_TAB_HELPER_DELEGATE_H_
