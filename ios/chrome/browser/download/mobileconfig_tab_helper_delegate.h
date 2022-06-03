// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MOBILECONFIG_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MOBILECONFIG_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate for MobileConfigTabHelper class.
@protocol MobileConfigTabHelperDelegate

// Called to download .mobileconfig file, |fileURL| points to the .mobileconfig
// file that we are trying to download. |fileURL| cannot be nil.
- (void)presentMobileConfigAlertFromURL:(NSURL*)fileURL;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MOBILECONFIG_TAB_HELPER_DELEGATE_H_
