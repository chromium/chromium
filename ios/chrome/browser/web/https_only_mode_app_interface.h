// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_HTTPS_ONLY_MODE_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_WEB_HTTPS_ONLY_MODE_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// The app interface for HTTPS-Only mode tests.
@interface HttpsOnlyModeAppInterface : NSObject

+ (void)setHTTPSPortForTesting:(int)HTTPSPortForTesting;
+ (void)setHTTPPortForTesting:(int)HTTPPortForTesting;
+ (void)useFakeHTTPSForTesting:(bool)useFakeHTTPSForTesting;

@end

#endif  // IOS_CHROME_BROWSER_WEB_HTTPS_ONLY_MODE_APP_INTERFACE_H_
