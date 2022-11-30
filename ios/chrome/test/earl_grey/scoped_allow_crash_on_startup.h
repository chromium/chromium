// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_SCOPED_ALLOW_CRASH_ON_STARTUP_H_
#define IOS_CHROME_TEST_EARL_GREY_SCOPED_ALLOW_CRASH_ON_STARTUP_H_

#import <objc/runtime.h>

#import "ios/third_party/earl_grey2/src/CommonLib/GREYSwizzler.h"
#import "ios/third_party/edo/src/Service/Sources/EDOClientService.h"

// Swizzles away various crash and error handlers such that if the app being
// tested crashes on startup, it does not immediately fail the test.
//
// There may be at most one live instance of ScopedAllowCrashOnStartup at any
// given time.
class ScopedAllowCrashOnStartup {
 public:
  ScopedAllowCrashOnStartup();
  ~ScopedAllowCrashOnStartup();

  // Returns true if there is a live ScopedAllowCrashOnStartup instance.
  static bool IsActive();

 private:
  GREYSwizzler* swizzler_;
  EDOClientErrorHandler original_client_error_handler_;
};

#endif  // IOS_CHROME_TEST_EARL_GREY_SCOPED_ALLOW_CRASH_ON_STARTUP_H_
