// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_service_factory.h"

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Class used to test TailoredSecurityServiceFactory initialization.
class TailoredSecurityServiceFactoryTest : public PlatformTest {
 protected:
  TailoredSecurityServiceFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
};

// Checks that TailoredSecurityServiceFactory returns a null for an
// off-the-record profile, but returns a non-null instance for a regular
// profile.
TEST_F(TailoredSecurityServiceFactoryTest, OffTheRecordReturnsNull) {
  // The factory should return null for an off-the-record profile.
  EXPECT_FALSE(TailoredSecurityServiceFactory::GetForProfile(
      profile_->GetOffTheRecordProfile()));

  // There should be a non-null instance for a regular profile.
  EXPECT_TRUE(TailoredSecurityServiceFactory::GetForProfile(profile_.get()));
}
