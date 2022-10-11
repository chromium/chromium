// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/crash_report/crash_helper.h"

#import <Foundation/Foundation.h>

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/crash/core/app/crashpad.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/crash_report/chrome_crash_reporter_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace crash_helper {
namespace common {

const char kCrashReportsUploadingEnabledKey[] = "CrashReportsUploadingEnabled";

const char kCrashpadStartOnNextRun[] = "CrashpadStartOnNextRun";

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

bool CanUseCrashpad() {
  static const bool can_use_crashpad = ([]() {
    @autoreleasepool {
      NSNumber* ns_value = [app_group::GetGroupUserDefaults()
          objectForKey:base::SysUTF8ToNSString(kCrashpadStartOnNextRun)];
      // CrashpadIOSEnabler is enabled by default, so treat nil as enabled.
      return ns_value == nil || ns_value.boolValue;
    }
  })();
  return can_use_crashpad;
}

base::FilePath CrashpadDumpLocation() {
  NSString* path = [app_group::CrashpadFolder() path];
  if (![path length]) {
    NSArray* cachesDirectories = NSSearchPathForDirectoriesInDomains(
        NSCachesDirectory, NSUserDomainMask, YES);
    NSString* cachePath = [cachesDirectories objectAtIndex:0];
    return base::FilePath(base::SysNSStringToUTF8(cachePath))
        .Append(kCrashpadNoAppGroupFolder);
  }
  return base::FilePath(base::SysNSStringToUTF8(path));
}

bool StartCrashpad() {
  ChromeCrashReporterClient::Create();
  return crash_reporter::InitializeCrashpad(true, "");
}

}  // namespace common
}  // namespace crash_helper
