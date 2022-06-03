// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/safety_check/safety_check_utils.h"

#include "base/time/time.h"
#include "ios/chrome/browser/ui/settings/safety_check/safety_check_constants.h"
#include "ios/chrome/browser/upgrade/upgrade_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool PreviousSafetyCheckIssueFound() {
  // Verify if the last safety check found issues.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  base::Time lastCompletedCheck = base::Time::FromDoubleT(
      [defaults doubleForKey:kTimestampOfLastIssueFoundKey]);
  return lastCompletedCheck != base::Time();
}
