// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_refresh/app_refresh_provider.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using AppRefreshProviderTest = PlatformTest;

@interface TestAppRefreshProvider : AppRefreshProvider
// Make identifier writable.
@property(nonatomic, strong, readwrite) NSString* identifier;
@end

@implementation TestAppRefreshProvider
@synthesize identifier = _identifier;
@end

// Expect that refreshInterval has a default value when AppRefreshProvider is
// created.
TEST_F(AppRefreshProviderTest, VerifyInitializer) {
  AppRefreshProvider* provider = [[AppRefreshProvider alloc] init];
  EXPECT_EQ(provider.refreshInterval, base::Minutes(15));
}

// Test that isDue will be true if the provider was never run before.
TEST_F(AppRefreshProviderTest, VerifyLastRunTime) {
  // Clear key containing last run time.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:@"AppRefreshProvider_lastRun_TestIdentifier"];

  TestAppRefreshProvider* provider = [[TestAppRefreshProvider alloc] init];
  provider.identifier = @"TestIdentifier";
  EXPECT_TRUE(provider.isDue);
}
