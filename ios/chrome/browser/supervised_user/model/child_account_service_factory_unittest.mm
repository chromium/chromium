// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/child_account_service_factory.h"

#import "components/supervised_user/core/browser/child_account_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for testing ChildAccountServiceFactory class.
class ChildAccountServiceFactoryTest : public PlatformTest {
 public:
  ChildAccountServiceFactoryTest() {
    profile_ = TestProfileIOS::Builder().Build();
    profile_->CreateOffTheRecordBrowserStateWithTestingFactories();
  }

  ProfileIOS* GetRegularProfile() { return profile_.get(); }

  ProfileIOS* GetOffTheRecordProfile() {
    return profile_->GetOffTheRecordProfile();
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that ChildAccountServiceFactory creates
// ChildAccountService.
TEST_F(ChildAccountServiceFactoryTest, CreateService) {
  supervised_user::ChildAccountService* service =
      ChildAccountServiceFactory::GetForProfile(GetRegularProfile());
  EXPECT_TRUE(service);
}

// Tests that ChildAccountServiceFactory retuns null
// with an off-the-record ProfileIOS.
TEST_F(ChildAccountServiceFactoryTest, ReturnsNullOnOffTheRecordProfile) {
  ProfileIOS* otr_profile = GetOffTheRecordProfile();
  ASSERT_TRUE(otr_profile);
  supervised_user::ChildAccountService* service =
      ChildAccountServiceFactory::GetForProfile(otr_profile);
  EXPECT_FALSE(service);
}
