// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/real_time_url_lookup_service_factory.h"

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class RealTimeUrlLookupServiceFactoryTest : public PlatformTest {
 protected:
  RealTimeUrlLookupServiceFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
};

// Checks that RealTimeUrlLookupServiceFactory returns a null for an
// off-the-record profile, but returns a non-null instance for a regular
// profile.
TEST_F(RealTimeUrlLookupServiceFactoryTest, OffTheRecordReturnsNull) {
  // The factory should return null for an off-the-record profile.
  EXPECT_FALSE(RealTimeUrlLookupServiceFactory::GetForProfile(
      profile_->GetOffTheRecordProfile()));

  // There should be a non-null instance for a regular profile.
  EXPECT_TRUE(RealTimeUrlLookupServiceFactory::GetForProfile(profile_.get()));
}
