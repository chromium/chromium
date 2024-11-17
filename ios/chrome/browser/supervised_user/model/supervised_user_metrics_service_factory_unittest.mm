// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_metrics_service_factory.h"

#import "components/supervised_user/core/browser/supervised_user_metrics_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for testing SupervisedUserMetricsServiceFactory class.
class SupervisedUserMetricsServiceFactoryTest : public PlatformTest {
 protected:
  SupervisedUserMetricsServiceFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  // ProfileIOS needs thread.
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that SupervisedUserMetricsServiceFactory creates
// SupervisedUserSettingsService.
TEST_F(SupervisedUserMetricsServiceFactoryTest, CreateService) {
  supervised_user::SupervisedUserMetricsService* service =
      SupervisedUserMetricsServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);
}

// Tests that SupervisedUserMetricsServiceFactory returns null
// with an off-the-record ProfileIOS.
TEST_F(SupervisedUserMetricsServiceFactoryTest,
       ReturnsNullOnOffTheRecordBrowserState) {
  ProfileIOS* otr_profile =
      profile_->CreateOffTheRecordBrowserStateWithTestingFactories();
  CHECK(otr_profile);
  supervised_user::SupervisedUserMetricsService* service =
      SupervisedUserMetricsServiceFactory::GetForProfile(otr_profile);
  ASSERT_FALSE(service);
}
