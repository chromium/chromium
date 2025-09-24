// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "components/collaboration/public/features.h"
#import "components/collaboration/public/pref_names.h"
#import "components/data_sharing/public/features.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using data_sharing::features::kCollaborationEntrepriseV2;
using data_sharing::features::kDataSharingFeature;

@interface SharedTabGroupsPolicyTestCase : ChromeTestCase
@end

@implementation SharedTabGroupsPolicyTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kDataSharingFeature);
  config.features_enabled.push_back(kCollaborationEntrepriseV2);
  return config;
}

- (void)setUp {
  [super setUp];

  // Sign into a managed account.
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey signinWithFakeManagedIdentityInPersonalProfile:identity];
}

// Checks that disabling support for Shared tab groups with a managed account
// syncs to the pref.
- (void)testDisabledByPolicy {
  const std::string pref =
      collaboration::prefs::kSharedTabGroupsManagedAccountSetting;
  GREYAssertEqual(
      [ChromeEarlGrey userIntegerPref:pref], 0l /*enabled*/,
      @"Shared tab groups's managed account setting should be enabled (0).");

  GREYAssertTrue([TabGroupAppInterface isAllowedToJoinTabGroups], @"");
  GREYAssertTrue([TabGroupAppInterface isAllowedToShareTabGroups], @"");

  [TabGroupAppInterface setSharedTabGroupsManagedAccountPolicyEnabled:NO];

  GREYAssertEqual(
      [ChromeEarlGrey userIntegerPref:pref], 1l /*disabled*/,
      @"Shared tab groups's managed account setting should be enabled (1).");

  GREYAssertFalse([TabGroupAppInterface isAllowedToJoinTabGroups], @"");
  GREYAssertFalse([TabGroupAppInterface isAllowedToShareTabGroups], @"");
}

@end
