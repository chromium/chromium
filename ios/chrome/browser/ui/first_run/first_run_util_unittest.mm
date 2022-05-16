// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using FirstRunUtilTest = PlatformTest;

// Tests IsApplicationManaged() when the kPolicyLoaderIOSConfigurationKey value
// doesn't exist.
TEST_F(FirstRunUtilTest, TestBrowserManagedNotExist) {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  [userDefaults removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
  EXPECT_FALSE(IsApplicationManaged());
}

// Tests IsApplicationManaged() when the kPolicyLoaderIOSConfigurationKey value
// is empty.
TEST_F(FirstRunUtilTest, TestBrowserManagedEmpty) {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  [userDefaults setObject:@{} forKey:kPolicyLoaderIOSConfigurationKey];
  EXPECT_FALSE(IsApplicationManaged());
  [userDefaults removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
}

// Tests IsApplicationManaged() when the kPolicyLoaderIOSConfigurationKey value
// is not empty.
TEST_F(FirstRunUtilTest, TestBrowserManagedWithValue) {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  NSDictionary* dict = @{@"key" : @"value"};
  [userDefaults setObject:dict forKey:kPolicyLoaderIOSConfigurationKey];
  EXPECT_TRUE(IsApplicationManaged());
  [userDefaults removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
}
