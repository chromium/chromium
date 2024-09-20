// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/verdict_cache_manager_factory.h"

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

using VerdictCacheManagerFactoryTest = PlatformTest;

// Checks that VerdictCacheManagerFactory returns different instances
// for an off-the-record profile and a regular profile.
TEST_F(VerdictCacheManagerFactoryTest, OffTheRecordUsesDifferentInstance) {
  web::WebTaskEnvironment task_environment;

  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  // There should be a non-null instance for an off-the-record profile.
  EXPECT_TRUE(VerdictCacheManagerFactory::GetForProfile(
      profile->GetOffTheRecordProfile()));

  EXPECT_NE(VerdictCacheManagerFactory::GetForProfile(profile.get()),
            VerdictCacheManagerFactory::GetForProfile(
                profile->GetOffTheRecordProfile()));
}
