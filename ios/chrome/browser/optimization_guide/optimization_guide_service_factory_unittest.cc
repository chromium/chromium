// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/optimization_guide/optimization_guide_service_factory.h"

#include "base/test/task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/optimization_guide/optimization_guide_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class OptimizationGuideServiceFactoryTest : public PlatformTest {
 public:
  OptimizationGuideServiceFactoryTest() {
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();
  }

  ChromeBrowserState* browser_state() { return browser_state_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
};

TEST_F(OptimizationGuideServiceFactoryTest, CheckNormalServiceNotNull) {
  EXPECT_NE(nullptr, OptimizationGuideServiceFactory::GetForBrowserState(
                         browser_state()));
}

TEST_F(OptimizationGuideServiceFactoryTest, CheckIncogitoServiceNull) {
  EXPECT_EQ(nullptr, OptimizationGuideServiceFactory::GetForBrowserState(
                         browser_state()->GetOffTheRecordChromeBrowserState()));
}

}  // namespace
