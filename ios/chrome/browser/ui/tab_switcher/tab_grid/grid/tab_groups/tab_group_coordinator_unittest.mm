// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_coordinator.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

class TabGroupCoordinatorTest : public PlatformTest {
 protected:
  TabGroupCoordinatorTest() {
    feature_list_.InitWithFeatures({kTabGroupsInGrid}, {});
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    base_view_controller_ = [[UIViewController alloc] init];
    coordinator_ = [[TabGroupCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];

    [coordinator_ start];
  }

  ~TabGroupCoordinatorTest() override { [coordinator_ stop]; }

  // Needed for test browser state created by TestBrowser().
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  TabGroupCoordinator* coordinator_;
};

TEST_F(TabGroupCoordinatorTest, TabGroupCoordinatorCreated) {
  ASSERT_NE(coordinator_, nil);
}
