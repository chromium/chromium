// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/site_settings/coordinator/site_settings_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/settings/site_settings/coordinator/site_settings_coordinator_delegate.h"
#import "ios/chrome/browser/settings/site_settings/public/site_settings_constants.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_category_detail_view_controller.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/fakes/fake_ui_navigation_controller.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test fixture for `SiteSettingsCoordinator`.
class SiteSettingsCoordinatorTest : public PlatformTest {
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

// Test that starting the coordinator pushes `SiteSettingsTableViewController`
// and selecting categories records `IOS.SiteSettings.CategoryOpened`.
TEST_F(SiteSettingsCoordinatorTest, TestSelectCategoryRecordsHistogram) {
  base::HistogramTester histogram_tester;
  SiteSettingsCoordinator* coordinator = [[SiteSettingsCoordinator alloc]
      initWithBaseNavigationController:base_navigation_controller_
                               browser:browser_.get()];

  [coordinator start];

  ASSERT_EQ(1u, [base_navigation_controller_.viewControllers count]);
  SiteSettingsTableViewController* view_controller =
      base::apple::ObjCCastStrict<SiteSettingsTableViewController>(
          base_navigation_controller_.topViewController);

  [view_controller.delegate
      siteSettingsTableViewController:view_controller
                    didSelectCategory:SiteSettingsCategory::kMicrophone];
  histogram_tester.ExpectUniqueSample("IOS.SiteSettings.CategoryOpened",
                                      SiteSettingsCategory::kMicrophone, 1);
  EXPECT_TRUE([base_navigation_controller_.topViewController
      isKindOfClass:[SiteSettingsCategoryDetailViewController class]]);

  [view_controller.delegate
      siteSettingsTableViewController:view_controller
                    didSelectCategory:SiteSettingsCategory::kCamera];
  histogram_tester.ExpectBucketCount("IOS.SiteSettings.CategoryOpened",
                                     SiteSettingsCategory::kCamera, 1);
  histogram_tester.ExpectTotalCount("IOS.SiteSettings.CategoryOpened", 2);

  [coordinator stop];
}
