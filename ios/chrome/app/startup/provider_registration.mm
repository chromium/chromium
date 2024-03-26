// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/provider_registration.h"

#import "base/system/sys_info.h"
#import "components/crash/core/app/crashpad.h"
#import "ios/public/provider/chrome/browser/app_utils/app_utils_api.h"
#import "ios/public/provider/chrome/browser/raccoon/raccoon_api.h"

@implementation ProviderRegistration

+ (void)registerProviders {
  // Needs to happen before any function of the provider API is used.
  ios::provider::Initialize();

  if (ios::provider::IsRaccoonEnabled()) {
    const std::string name = "iPad0,0";
    // Reset the "platform" value in the crash report that was initialized by
    // StartCrashController().
    crash_reporter::OverridePlatformValue(name);

    // Override the hardware model name. This will be used in
    // base::SysInfo::HardwareModelName().
    base::SysInfo::OverrideHardwareModelName(std::move(name));
  }
}

@end
