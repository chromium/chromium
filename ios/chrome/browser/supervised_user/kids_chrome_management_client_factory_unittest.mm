// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/kids_chrome_management_client_factory.h"

#import "components/supervised_user/core/browser/kids_chrome_management_client.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for testing KidsChromeManagementClientFactory class.
class KidsChromeManagementClientFactoryTest : public PlatformTest {
 protected:
  KidsChromeManagementClientFactoryTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {}

  // ChromeBrowserState needs thread.
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
};

// Tests that KidsChromeManagementClientFactory creates
// KidsChromeManagementClient.
TEST_F(KidsChromeManagementClientFactoryTest, CreateService) {
  KidsChromeManagementClient* service =
      KidsChromeManagementClientFactory::GetForBrowserState(
          browser_state_.get());
  ASSERT_TRUE(service);
}
