// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/model/breadcrumbs/application_breadcrumbs_logger.h"

#import <UIKit/UIKit.h>

#import <list>
#import <string>

#import "base/containers/contains.h"
#import "base/files/scoped_temp_dir.h"
#import "base/test/task_environment.h"
#import "components/breadcrumbs/core/breadcrumb_manager.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using breadcrumbs::BreadcrumbManager;

// Test fixture for testing ApplicationBreadcrumbsLogger class.
class ApplicationBreadcrumbsLoggerTest : public PlatformTest {
 protected:
  ApplicationBreadcrumbsLoggerTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    logger_ =
        std::make_unique<ApplicationBreadcrumbsLogger>(temp_dir_.GetPath());
  }

  base::test::TaskEnvironment task_environment_;

  // Observes orientation events and logs them to `breadcrumb_manager_`.
  std::unique_ptr<ApplicationBreadcrumbsLogger> logger_;
  base::ScopedTempDir temp_dir_;
};

// Tests logging device orientation.
TEST_F(ApplicationBreadcrumbsLoggerTest, Orientation) {
  const auto& events = BreadcrumbManager::GetInstance().GetEvents();
  ASSERT_EQ(1u, events.size());
  ASSERT_TRUE(base::Contains(events.back(), "Startup"));

  [NSNotificationCenter.defaultCenter
      postNotificationName:UIDeviceOrientationDidChangeNotification
                    object:nil];

  ASSERT_EQ(2u, events.size());

  EXPECT_TRUE(base::Contains(events.back(), kBreadcrumbOrientation))
      << events.back();

  // Ensure that same orientation is not logged more than once.
  [NSNotificationCenter.defaultCenter
      postNotificationName:UIDeviceOrientationDidChangeNotification
                    object:nil];
  EXPECT_EQ(2u, BreadcrumbManager::GetInstance().GetEvents().size());
}
