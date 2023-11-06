// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/safety_check/safety_check_utils.h"

#import "base/time/time.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_constants.h"
#import "ios/chrome/browser/upgrade/model/upgrade_utils.h"

bool PreviousSafetyCheckIssueFound() {
  // Verify if the last safety check found issues.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  base::Time lastCompletedCheck = base::Time::FromSecondsSinceUnixEpoch(
      [defaults doubleForKey:kTimestampOfLastIssueFoundKey]);
  return lastCompletedCheck != base::Time();
}
