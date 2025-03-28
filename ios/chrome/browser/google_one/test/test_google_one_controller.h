// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GOOGLE_ONE_TEST_TEST_GOOGLE_ONE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_GOOGLE_ONE_TEST_TEST_GOOGLE_ONE_CONTROLLER_H_

#import "ios/public/provider/chrome/browser/google_one/google_one_api.h"

// A Google One controller that just presents a red screen.
@interface TestGoogleOneController : NSObject <GoogleOneController>

@end

// A Google One controller factory that will return a TestGoogleOneController.
@interface TestGoogleOneControllerFactory
    : NSObject <GoogleOneControllerFactory>
+ (instancetype)sharedInstance;
@end

#endif  // IOS_CHROME_BROWSER_GOOGLE_ONE_TEST_TEST_GOOGLE_ONE_CONTROLLER_H_
