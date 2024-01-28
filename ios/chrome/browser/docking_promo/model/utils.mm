// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/model/utils.h"

#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"

BOOL CanShowDockingPromo(base::TimeDelta time_since_last_foreground) {
  if (IsChromeLikelyDefaultBrowser()) {
    return NO;
  }

  // For users no older than 2 days, whether they're active on their first day,
  // but not their second day.
  BOOL second_day_inactive =
      IsFirstRunRecent(base::Days(2) + base::Seconds(1)) &&
      (time_since_last_foreground > base::Days(1));

  // For users no older than 14 days, whether they've been inactive for 3
  // consecutive (or more) days.
  BOOL three_or_more_days_inactive =
      IsFirstRunRecent(base::Days(14) + base::Seconds(1)) &&
      (time_since_last_foreground > base::Days(3));

  return second_day_inactive || three_or_more_days_inactive;
}
