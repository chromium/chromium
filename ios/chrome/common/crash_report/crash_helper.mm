// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/common/crash_report/crash_helper.h"

#import <Foundation/Foundation.h>

#include "base/feature_list.h"
#include "base/strings/sys_string_conversions.h"
#include "components/crash/core/app/crashpad.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/common/crash_report/chrome_crash_reporter_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace crash_helper {
namespace common {

const char kCrashReportsUploadingEnabledKey[] = "CrashReportsUploadingEnabled";

const char kCrashpadStartOnNextRun[] = "CrashpadStartOnNextRun";

bool UserEnabledUploading() {
  return [app_group::GetGroupUserDefaults()
      boolForKey:base::SysUTF8ToNSString(kCrashReportsUploadingEnabledKey)];
}

bool CanCrashpadStart() {
  static bool can_crashpad_start = [app_group::GetGroupUserDefaults()
      boolForKey:base::SysUTF8ToNSString(kCrashpadStartOnNextRun)];
  return can_crashpad_start;
}

base::FilePath CrashpadDumpLocation() {
  return base::FilePath(
      base::SysNSStringToUTF8([app_group::CrashpadFolder() path]));
}

void StartCrashpad() {
  ChromeCrashReporterClient::Create();
  crash_reporter::InitializeCrashpad(true, "");
}

}  // namespace common
}  // namespace crash_helper
