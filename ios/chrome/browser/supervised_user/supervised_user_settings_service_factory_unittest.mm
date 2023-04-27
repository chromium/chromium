// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/supervised_user_settings_service_factory.h"

#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for testing SupervisedUserSettingsServiceFactory class.
class SupervisedUserSettingsServiceFactoryTest : public PlatformTest {
 protected:
  SupervisedUserSettingsServiceFactoryTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {}

  // ChromeBrowserState needs thread.
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
};

// Tests that SupervisedUserSettingsServiceFactory creates
// SupervisedUserSettingsService.
TEST_F(SupervisedUserSettingsServiceFactoryTest, CreateService) {
  supervised_user::SupervisedUserSettingsService* service =
      SupervisedUserSettingsServiceFactory::GetForBrowserState(
          browser_state_.get());
  ASSERT_TRUE(service);
}
