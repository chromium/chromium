// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/site_settings/coordinator/site_settings_category_detail_coordinator.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/settings/site_settings/public/site_settings_constants.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_category_detail_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/fakes/fake_ui_navigation_controller.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test fixture for SiteSettingsCategoryDetailCoordinator.
class SiteSettingsCategoryDetailCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base_navigation_controller_ = [[FakeUINavigationController alloc] init];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  FakeUINavigationController* base_navigation_controller_;
};

// Tests that starting the coordinator pushes the category detail view
// controller.
TEST_F(SiteSettingsCategoryDetailCoordinatorTest, TestStart) {
  SiteSettingsCategoryDetailCoordinator* coordinator =
      [[SiteSettingsCategoryDetailCoordinator alloc]
          initWithBaseNavigationController:base_navigation_controller_
                                   browser:browser_.get()
                                  category:SiteSettingsCategory::kMicrophone];

  [coordinator start];

  EXPECT_EQ(1u, [base_navigation_controller_.viewControllers count]);
  EXPECT_TRUE([base_navigation_controller_.topViewController
      isKindOfClass:[SiteSettingsCategoryDetailViewController class]]);

  [coordinator stop];
}

// Tests that removing the view controller notifies the delegate.
TEST_F(SiteSettingsCategoryDetailCoordinatorTest, TestDelegateWasRemoved) {
  SiteSettingsCategoryDetailCoordinator* coordinator =
      [[SiteSettingsCategoryDetailCoordinator alloc]
          initWithBaseNavigationController:base_navigation_controller_
                                   browser:browser_.get()
                                  category:SiteSettingsCategory::kMicrophone];

  id delegateMock = OCMProtocolMock(
      @protocol(SiteSettingsCategoryDetailViewControllerDelegate));
  coordinator.delegate = delegateMock;

  [coordinator start];

  SiteSettingsCategoryDetailViewController* viewController =
      base::apple::ObjCCastStrict<SiteSettingsCategoryDetailViewController>(
          base_navigation_controller_.topViewController);

  OCMExpect([delegateMock
      siteSettingsCategoryDetailViewControllerWasRemoved:viewController]);

  [viewController didMoveToParentViewController:nil];

  EXPECT_OCMOCK_VERIFY(delegateMock);

  [coordinator stop];
}
