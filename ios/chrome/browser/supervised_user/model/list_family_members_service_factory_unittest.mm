// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/list_family_members_service_factory.h"

#import "components/supervised_user/core/browser/list_family_members_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for testing ListFamilyMembersServiceFactory class.
class ListFamilyMembersServiceFactoryTest : public PlatformTest {
 public:
  ListFamilyMembersServiceFactoryTest() {
    profile_ = (TestProfileIOS::Builder().Build());
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

// Tests that ListFamilyMembersServiceFactory creates
// ListFamilyMembersService.
TEST_F(ListFamilyMembersServiceFactoryTest, CreateService) {
  supervised_user::ListFamilyMembersService* service =
      ListFamilyMembersServiceFactory::GetForProfile(GetRegularProfile());
  EXPECT_TRUE(service);
}

// Tests that ListFamilyMembersServiceFactory retuns null
// with an off-the-record ProfileIOS.
TEST_F(ListFamilyMembersServiceFactoryTest,
       ReturnsNullOnOffTheRecordBrowserState) {
  ProfileIOS* otr_profile = GetOffTheRecordProfile();
  ASSERT_TRUE(otr_profile);
  supervised_user::ListFamilyMembersService* service =
      ListFamilyMembersServiceFactory::GetForProfile(otr_profile);
  EXPECT_FALSE(service);
}
