// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/ohttp_key_service_factory.h"

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class OhttpKeyServiceFactoryTest : public PlatformTest {
 protected:
  OhttpKeyServiceFactoryTest() = default;

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

// Checks that OhttpKeyServiceFactory returns null for an off-the-record
// profile, but returns a non-null instance for a regular profile.
TEST_F(OhttpKeyServiceFactoryTest, GetForProfile) {
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(OhttpKeyServiceFactory::GetInstance(),
                            OhttpKeyServiceFactory::GetDefaultFactory());
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();

  // The factory should return null for an off-the-record profile.
  EXPECT_FALSE(
      OhttpKeyServiceFactory::GetForProfile(profile->GetOffTheRecordProfile()));

  // There should be a non-null instance for a regular profile.
  EXPECT_TRUE(OhttpKeyServiceFactory::GetForProfile(profile.get()));
}

// Checks that by default OhttpKeyServiceFactory returns null for both regular
// and off-the-record profiles for testing profiles if no testing factory is
// installed.
TEST_F(OhttpKeyServiceFactoryTest, GetForProfile_NoTestingFactory) {
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  // The factory should return null for an off-the-record profile.
  EXPECT_FALSE(
      OhttpKeyServiceFactory::GetForProfile(profile->GetOffTheRecordProfile()));

  // There should be a non-null instance for a regular profile.
  EXPECT_FALSE(OhttpKeyServiceFactory::GetForProfile(profile.get()));
}
