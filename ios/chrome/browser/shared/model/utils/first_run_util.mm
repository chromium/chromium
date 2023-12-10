// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/utils/first_run_util.h"

#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"

BOOL IsFirstRun() {
  return FirstRun::IsChromeFirstRun() ||
         experimental_flags::AlwaysDisplayFirstRun();
}

BOOL IsFirstRunRecent(const base::TimeDelta& timeDelta) {
  // Use the first_run age to determine the user is new on this device.
  if (IsFirstRun()) {
    return YES;
  }
  std::optional<base::File::Info> info = FirstRun::GetSentinelInfo();
  if (!info.has_value()) {
    return NO;
  }
  base::Time first_run_time = info.value().creation_time;
  BOOL isFirstRunRecent = base::Time::Now() - first_run_time < timeDelta;
  return isFirstRunRecent;
}
