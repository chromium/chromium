// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/supervised_user_metrics_service_factory.h"

#import "components/supervised_user/core/browser/supervised_user_metrics_service.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for testing SupervisedUserMetricsServiceFactory class.
class SupervisedUserMetricsServiceFactoryTest : public PlatformTest {
 protected:
  SupervisedUserMetricsServiceFactoryTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {}

  // ChromeBrowserState needs thread.
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

// Tests that SupervisedUserMetricsServiceFactory creates
// SupervisedUserSettingsService.
TEST_F(SupervisedUserMetricsServiceFactoryTest, CreateService) {
  supervised_user::SupervisedUserMetricsService* service =
      SupervisedUserMetricsServiceFactory::GetForBrowserState(
          browser_state_.get());
  ASSERT_TRUE(service);
}

// Tests that SupervisedUserMetricsServiceFactory returns null
// with an off-the-record ChromeBrowserState.
TEST_F(SupervisedUserMetricsServiceFactoryTest,
       ReturnsNullOnOffTheRecordBrowserState) {
  ChromeBrowserState* otr_browser_state =
      browser_state_->CreateOffTheRecordBrowserStateWithTestingFactories({});
  CHECK(otr_browser_state);
  supervised_user::SupervisedUserMetricsService* service =
      SupervisedUserMetricsServiceFactory::GetForBrowserState(
          otr_browser_state);
  ASSERT_FALSE(service);
}
