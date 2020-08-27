// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/upgrade/upgrade_utils.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/upgrade/upgrade_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool IsAppUpToDate() {
  // See if the user is out of date based on current information.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* nextVersionString =
      [defaults stringForKey:kIOSChromeNextVersionKey];
  base::Version nextVersion =
      base::Version(base::SysNSStringToUTF8(nextVersionString));

  const base::Version& currentVersion = version_info::GetVersion();

  // TODO(crbug.com/1078782): Add max supported version support.
  if (nextVersion.IsValid() && nextVersion > currentVersion &&
      (nextVersion.components()[0] - currentVersion.components()[0]) < 9) {
    return NO;
  }
  return YES;
}
