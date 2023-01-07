// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_EGTEST_BUNDLE_MAIN_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_EGTEST_BUNDLE_MAIN_H_

#import <Foundation/Foundation.h>

// An object that can be set as the NSPrincipalClass for an EG2 test bundle and
// performs various startup-related tasks.  This is necessary because test
// bundles don't have a fixed entry point in the way that application bundles
// do.
@interface ChromeEGTestBundleMain : NSObject
@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EGTEST_BUNDLE_MAIN_H_
