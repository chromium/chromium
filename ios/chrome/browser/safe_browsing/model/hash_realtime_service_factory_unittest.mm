// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/hash_realtime_service_factory.h"

#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class HashRealTimeServiceFactoryTest : public PlatformTest {
 protected:
  HashRealTimeServiceFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
};

// Checks that HashRealTimeServiceFactory returns null for an
// off-the-record profile.
TEST_F(HashRealTimeServiceFactoryTest, DisabledForIncognitoMode) {
  // The factory should return null for an off-the-record profile.
  EXPECT_FALSE(HashRealTimeServiceFactory::GetForProfile(
      profile_->GetOffTheRecordProfile()));
}

// Checks that HashRealTimeServiceFactory returns a non-null instance for a
// regular profile.
TEST_F(HashRealTimeServiceFactoryTest, EnabledForRegularMode) {
  // There should be a non-null instance for a regular profile.
  EXPECT_TRUE(HashRealTimeServiceFactory::GetForProfile(profile_.get()));
}
