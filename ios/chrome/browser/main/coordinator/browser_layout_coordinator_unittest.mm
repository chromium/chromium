// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/coordinator/browser_layout_coordinator.h"

#import "ios/chrome/browser/browser_view/ui_bundled/fake_browser_view_controller.h"
#import "ios/chrome/browser/main/ui/browser_layout_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

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

// Tests that the incognito property is correctly set on the view controller.
TEST_F(BrowserLayoutCoordinatorTest, IncognitoState) {
  // Regular browser.
  BrowserLayoutCoordinator* coordinator =
      [[BrowserLayoutCoordinator alloc] initWithBrowser:browser_.get()];
  [coordinator start];
  EXPECT_FALSE(coordinator.viewController.incognito);
  [coordinator stop];

  // Incognito browser.
  ProfileIOS* incognito_profile = profile_->GetOffTheRecordProfile();
  std::unique_ptr<TestBrowser> incognito_test_browser =
      std::make_unique<TestBrowser>(incognito_profile);

  BrowserLayoutCoordinator* incognito_coordinator =
      [[BrowserLayoutCoordinator alloc]
          initWithBrowser:incognito_test_browser.get()];
  [incognito_coordinator start];
  EXPECT_TRUE(incognito_coordinator.viewController.incognito);
  [incognito_coordinator stop];
}

// Tests that setting browserViewController on the coordinator's view controller
// works as expected.
TEST_F(BrowserLayoutCoordinatorTest, BrowserViewControllerAssignment) {
  BrowserLayoutCoordinator* coordinator =
      [[BrowserLayoutCoordinator alloc] initWithBrowser:browser_.get()];
  [coordinator start];

  FakeBrowserViewController* bvc = [[FakeBrowserViewController alloc] init];
  coordinator.viewController.browserViewController = bvc;
  EXPECT_EQ(coordinator.viewController.browserViewController, bvc);

  [coordinator stop];
}

// Tests the tab strip visibility based on form factor.
TEST_F(BrowserLayoutCoordinatorTest, TabStripVisibility) {
  BrowserLayoutCoordinator* coordinator =
      [[BrowserLayoutCoordinator alloc] initWithBrowser:browser_.get()];
  [coordinator start];

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    EXPECT_NE(coordinator.viewController.tabStripViewController, nil);
  } else {
    EXPECT_EQ(coordinator.viewController.tabStripViewController, nil);
  }

  [coordinator stop];
}
