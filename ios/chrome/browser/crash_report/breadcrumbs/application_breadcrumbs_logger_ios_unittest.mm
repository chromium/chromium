// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/application_breadcrumbs_logger_ios.h"

#import <UIKit/UIKit.h>

#include <list>
#include <string>

#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for testing ApplicationBreadcrumbsLogger class.
class ApplicationBreadcrumbsLoggerIOSTest : public PlatformTest {
 protected:
  ApplicationBreadcrumbsLoggerIOSTest()
      : logger_(std::make_unique<ApplicationBreadcrumbsLoggerIOS>(
            &breadcrumb_manager_)) {}

  breadcrumbs::BreadcrumbManager breadcrumb_manager_;
  std::unique_ptr<ApplicationBreadcrumbsLoggerIOS> logger_;
};

// Tests logging device orientation.
TEST_F(ApplicationBreadcrumbsLoggerIOSTest, Orientation) {
  ASSERT_EQ(1U, breadcrumb_manager_.GetEvents(0).size());  // startup event

  [NSNotificationCenter.defaultCenter
      postNotificationName:UIDeviceOrientationDidChangeNotification
                    object:nil];

  const std::list<std::string>& events = breadcrumb_manager_.GetEvents(0);
  ASSERT_EQ(2ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbOrientation))
      << events.back();

  // Ensure that same orientation is not logged more than once.
  [NSNotificationCenter.defaultCenter
      postNotificationName:UIDeviceOrientationDidChangeNotification
                    object:nil];
  EXPECT_EQ(2ul, breadcrumb_manager_.GetEvents(0).size());
}
