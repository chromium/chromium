// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_delegate.h"

#import <memory>
#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/uuid.h"
#import "components/saved_tab_groups/saved_tab_group.h"
#import "components/saved_tab_groups/saved_tab_group_tab.h"
#import "components/saved_tab_groups/types.h"
#import "components/tab_groups/tab_group_color.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace tab_groups {

class IOSTabGroupSyncDelegateTest : public PlatformTest {
 public:
  IOSTabGroupSyncDelegateTest() {
    app_state_ = OCMClassMock([AppState class]);

    browser_state_ = TestChromeBrowserState::Builder().Build();
    scene_state_ =
        [[FakeSceneState alloc] initWithAppState:app_state_
                                    browserState:browser_state_.get()];
    browser_ =
        scene_state_.browserProviderInterface.mainBrowserProvider.browser;

    scene_state_same_browser_state_ =
        [[FakeSceneState alloc] initWithAppState:app_state_
                                    browserState:browser_state_.get()];
    browser_same_browser_state_ =
        scene_state_same_browser_state_.browserProviderInterface
            .mainBrowserProvider.browser;

    other_browser_state_ = TestChromeBrowserState::Builder().Build();
    other_scene_state_ =
        [[FakeSceneState alloc] initWithAppState:app_state_
                                    browserState:other_browser_state_.get()];
    other_browser_ =
        other_scene_state_.browserProviderInterface.mainBrowserProvider.browser;

    browser_list_ =
        BrowserListFactory::GetForBrowserState(browser_state_.get());
    browser_list_->AddBrowser(browser_);
    browser_list_->AddBrowser(browser_same_browser_state_);

    delegate_ = std::make_unique<IOSTabGroupSyncDelegate>(browser_state_.get());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  id app_state_;
  FakeSceneState* scene_state_;
  FakeSceneState* scene_state_same_browser_state_;
  FakeSceneState* other_scene_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  Browser* browser_;
  Browser* browser_same_browser_state_;
  std::unique_ptr<TestChromeBrowserState> other_browser_state_;
  Browser* other_browser_;
  raw_ptr<BrowserList> browser_list_;
  std::unique_ptr<IOSTabGroupSyncDelegate> delegate_;
};

// Tests adding a tab group when the currently foregrounded active scene is with
// the same browser state.
TEST_F(IOSTabGroupSyncDelegateTest, CreateTabGroupSameBrowserStateForeground) {
  OCMStub([app_state_ foregroundActiveScene])
      .andReturn(scene_state_same_browser_state_);
  NSArray* foreground_scenes =
      @[ scene_state_, scene_state_same_browser_state_, other_scene_state_ ];
  OCMStub([app_state_ foregroundScenes]).andReturn(foreground_scenes);

  std::vector<SavedTabGroupTab> tabs;
  SavedTabGroup saved_group(u"my group", TabGroupColorId::kBlue, tabs,
                            std::make_optional(1));
  delegate_->CreateLocalTabGroup(saved_group);

  // TODO(crbug.com/329631494): Check that the web state list contains the
  // browser.
}

// Tests adding a tab group when the currently foreground active scene is from
// another browser state.
TEST_F(IOSTabGroupSyncDelegateTest, CreateTabGroupOtherBrowserStateForeground) {
  OCMStub([app_state_ foregroundActiveScene]).andReturn(other_scene_state_);
  NSArray* foreground_scenes = @[ scene_state_, other_scene_state_ ];
  OCMStub([app_state_ foregroundScenes]).andReturn(foreground_scenes);

  std::vector<SavedTabGroupTab> tabs;
  SavedTabGroup saved_group(u"my group", TabGroupColorId::kBlue, tabs,
                            std::make_optional(1));
  delegate_->CreateLocalTabGroup(saved_group);

  // TODO(crbug.com/329631494): Check that the web state list contains the
  // browser.
}

// Tests adding a tab group when there is no currently foreground active scene,
// the only foreground scene is from another browser state and there is one
// scene in background.
TEST_F(IOSTabGroupSyncDelegateTest, CreateTabGroupBackgroundScene) {
  OCMStub([app_state_ foregroundActiveScene]).andReturn(nil);
  OCMStub([app_state_ foregroundScenes]).andReturn(@[ other_scene_state_ ]);
  NSArray* connected_scenes = @[ other_scene_state_, scene_state_ ];
  OCMStub([app_state_ connectedScenes]).andReturn(connected_scenes);

  std::vector<SavedTabGroupTab> tabs;
  SavedTabGroup saved_group(u"my group", TabGroupColorId::kBlue, tabs,
                            std::make_optional(1));
  delegate_->CreateLocalTabGroup(saved_group);

  // TODO(crbug.com/329631494): Check that the web state list contains the
  // browser.
}

TEST_F(IOSTabGroupSyncDelegateTest, CloseTabGroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "| a [0 b* c ] d", browser_->GetBrowserState()));

  WebStateList* web_state_list_same_browser_state =
      browser_same_browser_state_->GetWebStateList();
  WebStateListBuilderFromDescription builder_same_browser_state(
      web_state_list_same_browser_state);
  ASSERT_TRUE(builder_same_browser_state.BuildWebStateListFromDescription(
      "| [1 e* f ] g h ", browser_->GetBrowserState()));

  // TODO(crbug.com/329631494): Use the two group IDs to close them.
  LocalTabGroupID local_id_group_0 = TabGroupId::CreateEmpty();
  delegate_->CloseLocalTabGroup(local_id_group_0);
}

TEST_F(IOSTabGroupSyncDelegateTest, UpdateTabGroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "| a [0 b* c ] d", browser_->GetBrowserState()));

  WebStateList* web_state_list_same_browser_state =
      browser_same_browser_state_->GetWebStateList();
  WebStateListBuilderFromDescription builder_same_browser_state(
      web_state_list_same_browser_state);
  ASSERT_TRUE(builder_same_browser_state.BuildWebStateListFromDescription(
      "| [1 e* f ] g h ", browser_->GetBrowserState()));

  // TODO(crbug.com/329631494): Update the groups (adding/removing tabs,
  // updating URLs of existing tabs...) and check that the tabs are not
  // realized.
  std::vector<SavedTabGroupTab> tabs;
  SavedTabGroup saved_group(u"my group", TabGroupColorId::kBlue, tabs,
                            std::make_optional(1));
  delegate_->UpdateLocalTabGroup(saved_group);
}

}  // namespace tab_groups
