// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/crash_report_user_application_state.h"

#import "components/crash/core/common/crash_key.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CrashReportUserApplicationState

+ (CrashReportUserApplicationState*)sharedInstance {
  static crash_reporter::CrashKeyString<256> key("user_application_state");
  static CrashReportUserApplicationState* instance =
      [[CrashReportUserApplicationState alloc] initWithKey:key];
  return instance;
}

@end
