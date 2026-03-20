// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_coordinator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/platform_test.h"

// Test fixture for testing FullscreenCoordinator class.
class FullscreenCoordinatorTest : public PlatformTest {
 protected:
  FullscreenCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    view_controller_ = [[UIViewController alloc] init];
    coordinator_ = [[FullscreenCoordinator alloc]
        initWithBaseViewController:view_controller_
                           browser:browser_.get()];
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* view_controller_;
  FullscreenCoordinator* coordinator_;
};

// Tests that the coordinator can be started and stopped.
TEST_F(FullscreenCoordinatorTest, StartStop) {
  [coordinator_ start];
  [coordinator_ stop];
}
