// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/model/crash_report_user_application_state.h"

#import "components/crash/core/common/crash_key.h"

@implementation CrashReportUserApplicationState

+ (CrashReportUserApplicationState*)sharedInstance {
  static crash_reporter::CrashKeyString<256> key("user_application_state");
  static CrashReportUserApplicationState* instance =
      [[CrashReportUserApplicationState alloc] initWithKey:key];
  return instance;
}

@end
