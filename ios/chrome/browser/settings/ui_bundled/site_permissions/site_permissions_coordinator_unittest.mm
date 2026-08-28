// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_coordinator.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_coordinator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/fakes/fake_ui_navigation_controller.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class SitePermissionsCoordinatorTest : public PlatformTest {
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

// Test starting the coordinator pushes the view controller.
TEST_F(SitePermissionsCoordinatorTest, TestStart) {
  SitePermissionsCoordinator* coordinator = [[SitePermissionsCoordinator alloc]
      initWithBaseNavigationController:base_navigation_controller_
                               browser:browser_.get()];

  [coordinator start];

  EXPECT_EQ(1u, [base_navigation_controller_.viewControllers count]);
  EXPECT_TRUE([base_navigation_controller_.topViewController
      isKindOfClass:[SitePermissionsTableViewController class]]);

  [coordinator stop];
}

// Test stopping the coordinator cleans up properly.
TEST_F(SitePermissionsCoordinatorTest, TestStop) {
  SitePermissionsCoordinator* coordinator = [[SitePermissionsCoordinator alloc]
      initWithBaseNavigationController:base_navigation_controller_
                               browser:browser_.get()];

  [coordinator start];
  [coordinator stop];

  // Starting and stopping should not crash or leak.
}

// Test that removing view controller calls delegate.
TEST_F(SitePermissionsCoordinatorTest, TestDelegateWasRemoved) {
  SitePermissionsCoordinator* coordinator = [[SitePermissionsCoordinator alloc]
      initWithBaseNavigationController:base_navigation_controller_
                               browser:browser_.get()];

  id delegateMock =
      OCMProtocolMock(@protocol(SitePermissionsCoordinatorDelegate));
  coordinator.delegate = delegateMock;

  [coordinator start];

  SitePermissionsTableViewController* tableController =
      base::apple::ObjCCastStrict<SitePermissionsTableViewController>(
          base_navigation_controller_.topViewController);

  OCMExpect([delegateMock sitePermissionsCoordinatorWasRemoved:coordinator]);

  [tableController didMoveToParentViewController:nil];

  EXPECT_OCMOCK_VERIFY(delegateMock);

  [coordinator stop];
}
