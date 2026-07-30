// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/fakes/fake_ui_navigation_controller.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class SuggestionsFromGeminiCoordinatorTest : public PlatformTest {
 protected:
  SuggestionsFromGeminiCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    navigation_controller_ = [[FakeUINavigationController alloc] init];
    coordinator_ = [[SuggestionsFromGeminiCoordinator alloc]
        initWithBaseNavigationController:navigation_controller_
                                 browser:browser_.get()];
  }

  ~SuggestionsFromGeminiCoordinatorTest() override { [coordinator_ stop]; }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  FakeUINavigationController* navigation_controller_;
  SuggestionsFromGeminiCoordinator* coordinator_;
};

// Tests that the coordinator starts and pushes its view controller.
TEST_F(SuggestionsFromGeminiCoordinatorTest, TestStartStop) {
  EXPECT_EQ(0u, navigation_controller_.viewControllers.count);
  [coordinator_ start];
  EXPECT_EQ(1u, navigation_controller_.viewControllers.count);
  [coordinator_ stop];
}

}  // namespace
