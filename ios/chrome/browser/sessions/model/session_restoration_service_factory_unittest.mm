// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class SessionRestorationServiceFactoryTest : public PlatformTest {
 public:
  SessionRestorationServiceFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  ProfileIOS* profile() { return profile_.get(); }

  ProfileIOS* otr_profile() { return profile_->GetOffTheRecordProfile(); }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that the factory correctly instantiate a new service for regular
// profile.
TEST_F(SessionRestorationServiceFactoryTest, CreateInstance) {
  EXPECT_TRUE(SessionRestorationServiceFactory::GetForProfile(profile()));
}

// Tests that the factory correctly instantiate a new service for off-the-record
// profile.
TEST_F(SessionRestorationServiceFactoryTest, CreateOTRInstance) {
  EXPECT_TRUE(SessionRestorationServiceFactory::GetForProfile(otr_profile()));
}

// Tests that regular and off-the-record profiles uses distinct instances.
TEST_F(SessionRestorationServiceFactoryTest, InstancesAreDistinct) {
  EXPECT_NE(SessionRestorationServiceFactory::GetForProfile(profile()),
            SessionRestorationServiceFactory::GetForProfile(otr_profile()));
}
