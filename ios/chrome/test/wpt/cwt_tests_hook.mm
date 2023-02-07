// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/tests_hook.h"

#import "ios/chrome/test/wpt/cwt_constants.h"
#import "ios/chrome/test/wpt/cwt_webdriver_app_interface.h"
#import "ios/third_party/edo/src/Service/Sources/EDOHostService.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace tests_hook {

bool DisableAppGroupAccess() {
  return true;
}
bool DisableClientSideFieldTrials() {
  return true;
}
bool DisableContentSuggestions() {
  return true;
}
bool DisableDiscoverFeed() {
  return true;
}
bool DisableFirstRun() {
  return true;
}
bool DisableGeolocation() {
  return true;
}
bool DisablePromoManagerFullScreenPromos() {
  return true;
}
bool DisableUpgradeSigninPromo() {
  return true;
}
bool DisableUpdateService() {
  return true;
}
bool DisableMainThreadFreezeDetection() {
  return true;
}
policy::ConfigurationPolicyProvider* GetOverriddenPlatformPolicyProvider() {
  return nullptr;
}
std::unique_ptr<SystemIdentityManager> CreateSystemIdentityManager() {
  return nullptr;
}
void SetUpTestsIfPresent() {
  CWTWebDriverAppInterface* appInterface =
      [[CWTWebDriverAppInterface alloc] init];
  [EDOHostService serviceWithPort:kCwtEdoPortNumber
                       rootObject:appInterface
                            queue:[appInterface executingQueue]];
}

void RunTestsIfPresent() {}

}  // namespace tests_hook
