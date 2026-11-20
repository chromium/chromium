// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator+subclassing.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class ChromeCoordinatorTest : public PlatformTest {
 protected:
  ChromeCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    base_view_controller_ = [[UIViewController alloc] init];
  }

  void TearDown() override {
    [scene_state_ shutdown];
    PlatformTest::TearDown();
  }

  Browser* regular_browser() {
    return scene_state_.browserProviderInterface.mainBrowser;
  }

  Browser* incognito_browser() {
    return scene_state_.browserProviderInterface.incognitoBrowser;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeSceneState* scene_state_;
  UIViewController* base_view_controller_;
};

// Tests that a coordinator initialized with a Browser uses that Browser, even
// if it is the incognito Browser, and the provided base view controller.
TEST_F(ChromeCoordinatorTest, InitWithBrowserUsesProvidedBrowser) {
  ChromeCoordinator* coordinator = [[ChromeCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:incognito_browser()];

  EXPECT_EQ(coordinator.browser, incognito_browser());
  EXPECT_EQ(coordinator.baseViewController, base_view_controller_);
}

// Tests that a coordinator initialized with the regular Browser of a scene uses
// that scene's regular Browser, even while its incognito Browser is current,
// and the provided base view controller.
TEST_F(ChromeCoordinatorTest, InitWithRegularBrowserOfSceneUsesRegularBrowser) {
  [scene_state_ setCurrentBrowser:incognito_browser()];

  ChromeCoordinator* coordinator = [[ChromeCoordinator alloc]
      initWithBaseViewController:base_view_controller_
           regularBrowserOfScene:scene_state_];

  EXPECT_EQ(coordinator.browser, regular_browser());
  EXPECT_EQ(coordinator.baseViewController, base_view_controller_);
}

// Tests that a coordinator initialized with a regular Browser uses it and the
// provided base view controller.
TEST_F(ChromeCoordinatorTest, InitWithRegularBrowserUsesProvidedBrowser) {
  ChromeCoordinator* coordinator = [[ChromeCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                  regularBrowser:regular_browser()];

  EXPECT_EQ(coordinator.browser, regular_browser());
  EXPECT_EQ(coordinator.baseViewController, base_view_controller_);
}

}  // namespace
