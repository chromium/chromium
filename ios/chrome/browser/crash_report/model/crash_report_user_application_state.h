// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_REPORT_USER_APPLICATION_STATE_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_REPORT_USER_APPLICATION_STATE_H_

#import "ios/chrome/browser/crash_report/model/crash_report_multi_parameter.h"

// This class is a singleton to manage the user_application_state element of the
// crash report.
@interface CrashReportUserApplicationState : CrashReportMultiParameter

+ (CrashReportUserApplicationState*)sharedInstance;

@end

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_REPORT_USER_APPLICATION_STATE_H_
