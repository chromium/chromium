// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"

#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for testing SupervisedUserSettingsServiceFactory class.
class SupervisedUserSettingsServiceFactoryTest : public PlatformTest {
 protected:
  SupervisedUserSettingsServiceFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  // ProfileIOS needs thread.
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
};

// Tests that SupervisedUserSettingsServiceFactory creates
// SupervisedUserSettingsService.
TEST_F(SupervisedUserSettingsServiceFactoryTest, CreateService) {
  supervised_user::SupervisedUserSettingsService* service =
      SupervisedUserSettingsServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);
}
