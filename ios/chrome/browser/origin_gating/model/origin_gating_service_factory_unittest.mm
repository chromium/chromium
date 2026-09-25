// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/origin_gating/model/origin_gating_service_factory.h"

#import <memory>

#import "components/origin_gating/core/origin_gating_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace origin_gating {
namespace {

class OriginGatingServiceFactoryTest : public PlatformTest {
 protected:
  web::WebTaskEnvironment task_environment_;
};

TEST_F(OriginGatingServiceFactoryTest, SameProfile) {
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  OriginGatingService* service1 =
      OriginGatingServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(service1, nullptr);

  OriginGatingService* service2 =
      OriginGatingServiceFactory::GetForProfile(profile.get());
  EXPECT_EQ(service1, service2);
}

TEST_F(OriginGatingServiceFactoryTest, UnrelatedProfiles) {
  std::unique_ptr<TestProfileIOS> profile1 = TestProfileIOS::Builder().Build();
  std::unique_ptr<TestProfileIOS> profile2 = TestProfileIOS::Builder().Build();

  OriginGatingService* service1 =
      OriginGatingServiceFactory::GetForProfile(profile1.get());
  ASSERT_NE(service1, nullptr);

  OriginGatingService* service2 =
      OriginGatingServiceFactory::GetForProfile(profile2.get());
  ASSERT_NE(service2, nullptr);

  // Different/unrelated profiles get distinct service instances.
  EXPECT_NE(service1, service2);
}

TEST_F(OriginGatingServiceFactoryTest, OffTheRecordProfile) {
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();
  ProfileIOS* otr_profile = profile->GetOffTheRecordProfile();

  OriginGatingService* regular_service =
      OriginGatingServiceFactory::GetForProfile(profile.get());
  ASSERT_NE(regular_service, nullptr);

  OriginGatingService* otr_service =
      OriginGatingServiceFactory::GetForProfile(otr_profile);
  ASSERT_NE(otr_service, nullptr);

  // Off-the-record profiles get their own distinct instance
  EXPECT_NE(regular_service, otr_service);

  // Calling again on the same OTR profile returns the same instance.
  EXPECT_EQ(otr_service,
            OriginGatingServiceFactory::GetForProfile(otr_profile));
}

}  // namespace
}  // namespace origin_gating
