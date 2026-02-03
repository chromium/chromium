// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/coordinator/browser_layout_coordinator.h"

#import "ios/chrome/browser/main/ui/browser_layout_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class BrowserLayoutCoordinatorTest : public PlatformTest {
 protected:
  BrowserLayoutCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests that the coordinator can be started and stopped without crashing.
TEST_F(BrowserLayoutCoordinatorTest, StartStop) {
  BrowserLayoutCoordinator* coordinator =
      [[BrowserLayoutCoordinator alloc] initWithBrowser:browser_.get()];

  [coordinator start];

  // Verify that the view controller is created.
  EXPECT_TRUE(coordinator.viewController);
  EXPECT_TRUE([coordinator.viewController
      isKindOfClass:[BrowserLayoutViewController class]]);

  [coordinator stop];

  // Verify that the view controller is released/cleared.
  EXPECT_FALSE(coordinator.viewController);
}
