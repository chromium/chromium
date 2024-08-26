// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_notification/model/content_notification_service_factory.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class ContentNotificationServiceFactoryTest : public PlatformTest {
 public:
  ContentNotificationServiceFactoryTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {}

  ChromeBrowserState* browser_state() { return browser_state_.get(); }

  ChromeBrowserState* otr_browser_state() {
    return browser_state_->GetOffTheRecordChromeBrowserState();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

// Tests that the factory returns a non-null instance for regular BrowserStates.
TEST_F(ContentNotificationServiceFactoryTest, CreateInstance) {
  ContentNotificationService* const service =
      ContentNotificationServiceFactory::GetForBrowserState(browser_state());
  EXPECT_NE(service, nullptr);
}

// Tests that the factory returns a null instance for off-the-record
// BrowserStates.
TEST_F(ContentNotificationServiceFactoryTest, CreateOTRInstance) {
  ContentNotificationService* const service =
      ContentNotificationServiceFactory::GetForBrowserState(
          otr_browser_state());
  EXPECT_EQ(service, nullptr);
}
