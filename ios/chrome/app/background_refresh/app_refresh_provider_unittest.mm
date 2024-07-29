// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_refresh/app_refresh_provider.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using AppRefreshProviderTest = PlatformTest;

// Expect that refreshInterval has a default value when AppRefreshProvider is
// created.
TEST_F(AppRefreshProviderTest, VerifyInitializer) {
  AppRefreshProvider* provider = [[AppRefreshProvider alloc] init];
  EXPECT_EQ(provider.refreshInterval, base::Minutes(15));
}
