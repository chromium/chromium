// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_SCOPED_ALLOW_CRASH_ON_STARTUP_H_
#define IOS_CHROME_TEST_EARL_GREY_SCOPED_ALLOW_CRASH_ON_STARTUP_H_

#import <objc/runtime.h>

#import "ios/third_party/edo/src/Service/Sources/EDOClientService.h"

// Configures the enclosing test fixture to allow Chrome to crash on restart.
// This disables error handling that would otherwise automatically fail the
// test if Chrome fails to start. It also skips over some startup services
// that may attempt to communicate with a (potentially non-existent) Chrome
// instance.
//
// When ScopedAllowCrashOnStartup is used, you probably want to use
// ChromeTestCase+testForStartup: as well.
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
  Method original_crash_handler_method_;
  Method swizzled_crash_handler_method_;
  EDOClientErrorHandler original_client_error_handler_;
};

#endif  // IOS_CHROME_TEST_EARL_GREY_SCOPED_ALLOW_CRASH_ON_STARTUP_H_
