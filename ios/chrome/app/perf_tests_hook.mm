// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/tests_hook.h"

#import "base/command_line.h"
#import "ios/chrome/browser/flags/chrome_switches.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace tests_hook {

bool DisableAppGroupAccess() {
  return false;
}
bool DisableClientSideFieldTrials() {
  // Always disable client side field trials for perf tests.
  return true;
}
bool DisableContentSuggestions() {
  return false;
}
bool DisableDiscoverFeed() {
  // Performance tests must explicitly enable the discover feed by passing the
  // --enable-discover-feed command line switch.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDiscoverFeed);
}
bool DisableFirstRun() {
  // Always disable FRE for perf tests.
  return true;
}
bool DisableGeolocation() {
  return false;
}
bool DisableUpgradeSigninPromo() {
  // Always disable upgrade sign-in promo for perf tests.
  return true;
}
bool DisableUpdateService() {
  return false;
}
bool DisableMainThreadFreezeDetection() {
  return false;
}
policy::ConfigurationPolicyProvider* GetOverriddenPlatformPolicyProvider() {
  return nullptr;
}
std::unique_ptr<SystemIdentityManager> CreateSystemIdentityManager() {
  return nullptr;
}
void SetUpTestsIfPresent() {}
void RunTestsIfPresent() {}

}  // namespace tests_hook
