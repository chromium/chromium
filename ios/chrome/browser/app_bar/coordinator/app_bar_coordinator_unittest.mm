// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_coordinator.h"

#import <memory>

#import "ios/chrome/browser/app_bar/ui/app_bar_container_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class AppBarCoordinatorTest : public PlatformTest {
 protected:
  AppBarCoordinatorTest() {
    regular_profile_ = TestProfileIOS::Builder().Build();
    incognito_profile_ = TestProfileIOS::Builder().Build();
    regular_browser_ = std::make_unique<TestBrowser>(regular_profile_.get());
    incognito_browser_ =
        std::make_unique<TestBrowser>(incognito_profile_.get());
    coordinator_ = [[AppBarCoordinator alloc]
        initWithRegularBrowser:regular_browser_.get()
              incognitoBrowser:incognito_browser_.get()];
    scene_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:scene_handler_
                     forProtocol:@protocol(SceneCommands)];
    tab_grid_handler_ = OCMProtocolMock(@protocol(TabGridCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:tab_grid_handler_
                     forProtocol:@protocol(TabGridCommands)];
    tab_group_handler_ = OCMProtocolMock(@protocol(TabGroupsCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:tab_group_handler_
                     forProtocol:@protocol(TabGroupsCommands)];
  }

  ~AppBarCoordinatorTest() override { [coordinator_ stop]; }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> regular_profile_;
  std::unique_ptr<TestProfileIOS> incognito_profile_;
  std::unique_ptr<TestBrowser> regular_browser_;
  std::unique_ptr<TestBrowser> incognito_browser_;
  AppBarCoordinator* coordinator_;
  id scene_handler_;
  id tab_grid_handler_;
  id tab_group_handler_;
};

// Tests that the coordinator creates a view controller when started.
TEST_F(AppBarCoordinatorTest, TestStart) {
  [coordinator_ start];
  EXPECT_TRUE(coordinator_.viewController);
  EXPECT_TRUE([coordinator_.viewController
      isKindOfClass:[AppBarContainerViewController class]]);
}
