// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/tests_hook.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/policy/test_platform_policy_provider.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/signin_test_util.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace tests_hook {

bool DisableAppGroupAccess() {
  return true;
}

bool DisableContentSuggestions() {
  return true;
}

bool DisableDiscoverFeed() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDiscoverFeed);
}

bool DisableFirstRun() {
  return true;
}

bool DisableGeolocation() {
  return true;
}

bool DisableUpgradeSigninPromo() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableUpgradeSigninPromo);
}

bool DisableUpdateService() {
  return true;
}

bool DisableMainThreadFreezeDetection() {
  return true;
}

policy::ConfigurationPolicyProvider* GetOverriddenPlatformPolicyProvider() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "com.apple.configuration.managed")) {
    DVLOG(1) << "Policy data present in NSUserDefaults, not installing test "
                "platform provider";
    return nullptr;
  }
  return GetTestPlatformPolicyProvider();
}

void SetUpTestsIfPresent() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          test_switches::kSignInAtStartup)) {
    // Record an identity as "known". If the identity isn't added, the
    // AuthenticationService will log the fake user off.
    std::unique_ptr<ios::FakeChromeIdentityService> service(
        new ios::FakeChromeIdentityService());
    service->SetUpForIntegrationTests();
    ios::GetChromeBrowserProvider().SetChromeIdentityServiceForTesting(
        std::move(service));
    ios::FakeChromeIdentityService* identity_service =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
    identity_service->AddIdentity([FakeChromeIdentity fakeIdentity1]);
  }
}

void RunTestsIfPresent() {
  // No-op for Earl Grey.
}

}  // namespace tests_hook
