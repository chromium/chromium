// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/policy_util.h"

#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "testing/platform_test.h"

using PolicyUtilTest = PlatformTest;

// Tests that IsApplicationManagedByPlatform() returns false when the
// kPolicyLoaderIOSConfigurationKey value doesn't exist.
TEST_F(PolicyUtilTest, ReturnsFalseWhenNoApplicationConfigFromPlatform) {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  [userDefaults removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
  EXPECT_FALSE(IsApplicationManagedByPlatform());
}

// Tests that IsApplicationManagedByPlatform() returns false when the
// kPolicyLoaderIOSConfigurationKey value is empty.
TEST_F(PolicyUtilTest, ReturnsFalseWhenEmptyApplicationConfigFromPlatform) {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  [userDefaults setObject:@{} forKey:kPolicyLoaderIOSConfigurationKey];
  EXPECT_FALSE(IsApplicationManagedByPlatform());
  [userDefaults removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
}

// Tests that IsApplicationManagedByPlatform() returns true when the
// kPolicyLoaderIOSConfigurationKey value is not empty.
TEST_F(PolicyUtilTest, ReturnsTrueWhenApplicationConfigFromPlatform) {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  NSDictionary* dict = @{@"key" : @"value"};
  [userDefaults setObject:dict forKey:kPolicyLoaderIOSConfigurationKey];
  EXPECT_TRUE(IsApplicationManagedByPlatform());
  [userDefaults removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
}
