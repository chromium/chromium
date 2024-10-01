// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_coordinator.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_holder.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using tab_groups::TabGroupId;

// Fake WebStateList delegate that attaches the required tab helper.
class TabGroupCoordinatorFakeWebStateListDelegate
    : public FakeWebStateListDelegate {
 public:
  TabGroupCoordinatorFakeWebStateListDelegate() {}
  ~TabGroupCoordinatorFakeWebStateListDelegate() override {}

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override {
    SnapshotTabHelper::CreateForWebState(web_state);
  }
};

class TabGroupCoordinatorTest : public PlatformTest {
 protected:
  TabGroupCoordinatorTest() {
    feature_list_.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(
        profile_.get(),
        std::make_unique<TabGroupCoordinatorFakeWebStateListDelegate>());

    SnapshotBrowserAgent::CreateForBrowser(browser_.get());

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();

    id mockTabGroupsCommandHandler =
        OCMProtocolMock(@protocol(TabGroupsCommands));
    [dispatcher startDispatchingToTarget:mockTabGroupsCommandHandler
                             forProtocol:@protocol(TabGroupsCommands)];

    base_view_controller_ = [[UIViewController alloc] init];

    tab_groups::TabGroupVisualData temporaryVisualData(
        u"Test group", tab_groups::TabGroupColorId::kCyan);
    WebStateList* web_state_list = browser_->GetWebStateList();
    web_state_list->InsertWebState(
        std::make_unique<web::FakeWebState>(web::WebStateID::NewUnique()),
        WebStateList::InsertionParams::Automatic().Activate());
    const TabGroup* group = web_state_list->CreateGroup(
        {0}, temporaryVisualData, TabGroupId::GenerateNew());

    mode_holder_ = [[TabGridModeHolder alloc] init];

    coordinator_ = [[TabGroupCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                          tabGroup:group];
    coordinator_.modeHolder = mode_holder_;

    [coordinator_ start];
  }

  ~TabGroupCoordinatorTest() override { [coordinator_ stop]; }

  // Needed for test profile created by TestBrowser().
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  TabGroupCoordinator* coordinator_;
  TabGridModeHolder* mode_holder_;
};

TEST_F(TabGroupCoordinatorTest, TabGroupCoordinatorCreated) {
  ASSERT_NE(coordinator_, nil);
}
