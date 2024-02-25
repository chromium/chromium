// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/utils/first_run_util.h"

#import <optional>

#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"

BOOL IsFirstRun() {
  return FirstRun::IsChromeFirstRun() ||
         experimental_flags::AlwaysDisplayFirstRun();
}

BOOL IsFirstRunRecent(const base::TimeDelta& time_delta) {
  // Use the first_run age to determine the user is new on this device.
  if (IsFirstRun()) {
    return YES;
  }

  std::optional<int> recency = experimental_flags::GetFirstRunRecency();
  if (recency.has_value()) {
    return base::Days(recency.value()) < time_delta;
  }

  std::optional<base::File::Info> info = FirstRun::GetSentinelInfo();
  if (!info.has_value()) {
    return NO;
  }
  base::Time first_run_time = info.value().creation_time;
  BOOL is_first_run_recent = base::Time::Now() - first_run_time < time_delta;
  return is_first_run_recent;
}
