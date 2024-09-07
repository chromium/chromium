// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/crash_report/crash_helper.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/crash/core/app/crashpad.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/crash_report/chrome_crash_reporter_client.h"

namespace crash_helper {
namespace common {

const char kCrashReportsUploadingEnabledKey[] = "CrashReportsUploadingEnabled";

const char kCrashpadNoAppGroupFolder[] = "Crashpad";

bool UserEnabledUploading() {
  return [app_group::GetGroupUserDefaults()
      boolForKey:base::SysUTF8ToNSString(kCrashReportsUploadingEnabledKey)];
}

void SetUserEnabledUploading(bool enabled) {
  [app_group::GetGroupUserDefaults()
      setBool:enabled ? YES : NO
       forKey:base::SysUTF8ToNSString(
                  common::kCrashReportsUploadingEnabledKey)];
}

base::FilePath CrashpadDumpLocation() {
  NSString* path = [app_group::CrashpadFolder() path];
  if (![path length]) {
    NSArray* cachesDirectories = NSSearchPathForDirectoriesInDomains(
        NSCachesDirectory, NSUserDomainMask, YES);
    NSString* cachePath = [cachesDirectories objectAtIndex:0];
    return base::apple::NSStringToFilePath(cachePath).Append(
        kCrashpadNoAppGroupFolder);
  }
  return base::apple::NSStringToFilePath(path);
}

bool StartCrashpad() {
  ChromeCrashReporterClient::Create();
  return crash_reporter::InitializeCrashpad(true, "");
}

}  // namespace common
}  // namespace crash_helper
