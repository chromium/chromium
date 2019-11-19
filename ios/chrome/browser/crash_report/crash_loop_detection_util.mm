// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/crash_loop_detection_util.h"

#import <Foundation/Foundation.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
static int startup_attempt_count = -1;
NSString* const kAppStartupFailureCountKey = @"AppStartupFailureCount";
}

namespace crash_util {

int GetFailedStartupAttemptCount() {
  if (startup_attempt_count == -1) {
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    startup_attempt_count = [defaults integerForKey:kAppStartupFailureCountKey];
  }
  return startup_attempt_count;
}

void IncrementFailedStartupAttemptCount(bool flush_immediately) {
  int startup_attempt_count = GetFailedStartupAttemptCount();
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setInteger:(startup_attempt_count + 1)
                forKey:kAppStartupFailureCountKey];
  if (flush_immediately)
    [defaults synchronize];
}

void ResetFailedStartupAttemptCount() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  if ([defaults integerForKey:kAppStartupFailureCountKey] != 0) {
    [defaults setInteger:0 forKey:kAppStartupFailureCountKey];
    [defaults synchronize];
  }
}

void ResetFailedStartupAttemptCountForTests() {
  ResetFailedStartupAttemptCount();
  startup_attempt_count = -1;
}

}  // namespace crash_util
