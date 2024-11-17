// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"

#import "base/memory/scoped_refptr.h"
#import "base/threading/thread_restrictions.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_notifier_impl.h"
#import "components/prefs/pref_value_store.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for testing SupervisedUserServiceFactory class.
class SupervisedUserServiceFactoryTest : public PlatformTest {
 public:
  SupervisedUserServiceFactoryTest() {
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

// Tests that SupervisedUserServiceFactory creates
// SupervisedUserService.
TEST_F(SupervisedUserServiceFactoryTest, CreateService) {
  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(GetRegularProfile());
  EXPECT_TRUE(service);
}

// Tests that SupervisedUserServiceFactory retuns null
// with an off-the-record Profile.
TEST_F(SupervisedUserServiceFactoryTest, ReturnsNullOnOffTheRecordProfile) {
  ProfileIOS* otr_profile = GetOffTheRecordProfile();
  ASSERT_TRUE(otr_profile);
  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(otr_profile);
  EXPECT_FALSE(service);
}

