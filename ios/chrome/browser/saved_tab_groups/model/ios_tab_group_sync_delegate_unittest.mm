// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_delegate.h"

#import <memory>
#import <optional>
#import <string>

#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/uuid.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/saved_tab_group_tab.h"
#import "components/saved_tab_groups/public/types.h"
#import "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#import "components/tab_groups/tab_group_color.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_action_context.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_local_update_observer.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/sessions/model/test_session_restoration_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using ::testing::_;
using ::testing::Property;
using ::testing::Return;

namespace tab_groups {

namespace {

std::unique_ptr<KeyedService> CreateMockSyncService(
    web::BrowserState* context) {
  return std::make_unique<::testing::NiceMock<MockTabGroupSyncService>>();
}

// Updates the association of the local tab id.
void FakeUpdateLocalTabId(web::WebState* web_state,
                          SavedTabGroupTab& saved_tab,
                          SavedTabGroup& saved_group) {
  saved_tab.SetLocalTabID(web_state->GetUniqueIdentifier().identifier());
  saved_group.UpdateTab(saved_tab);
}

}  // namespace

class IOSTabGroupSyncDelegateTest : public PlatformTest {
 public:
  IOSTabGroupSyncDelegateTest() {
    app_state_ = OCMClassMock([AppState class]);

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(TabGroupSyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        SessionRestorationServiceFactory::GetInstance(),
        TestSessionRestorationService::GetTestingFactory());
    profile_ = std::move(builder).Build();

    mock_service_ = static_cast<MockTabGroupSyncService*>(
        TabGroupSyncServiceFactory::GetForProfile(profile_.get()));

    scene_state_ = [[FakeSceneState alloc] initWithAppState:app_state_
                                                    profile:profile_.get()];
    browser_ =
        scene_state_.browserProviderInterface.mainBrowserProvider.browser;
    TabInsertionBrowserAgent::CreateForBrowser(browser_);

    scene_state_same_profile_ =
        [[FakeSceneState alloc] initWithAppState:app_state_
                                         profile:profile_.get()];
    browser_same_profile_ = scene_state_same_profile_.browserProviderInterface
                                .mainBrowserProvider.browser;
    TabInsertionBrowserAgent::CreateForBrowser(browser_same_profile_);

    other_profile_ = TestProfileIOS::Builder().Build();
    other_scene_state_ =
        [[FakeSceneState alloc] initWithAppState:app_state_
                                         profile:other_profile_.get()];
    other_browser_ =
        other_scene_state_.browserProviderInterface.mainBrowserProvider.browser;
    TabInsertionBrowserAgent::CreateForBrowser(other_browser_);
    other_inactive_browser_ = other_browser_->CreateInactiveBrowser();

    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());
    auto local_observer = std::make_unique<TabGroupLocalUpdateObserver>(
        browser_list_.get(), mock_service_);

    browser_list_->AddBrowser(browser_);
    browser_list_->AddBrowser(browser_same_profile_);

    delegate_ = std::make_unique<IOSTabGroupSyncDelegate>(
        browser_list_, mock_service_, std::move(local_observer));

    mock_application_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    mock_tab_groups_handler_ =
        OCMStrictProtocolMock(@protocol(TabGroupsCommands));
    mock_tab_grid_handler_ = OCMStrictProtocolMock(@protocol(TabGridCommands));
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mock_application_handler_
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher startDispatchingToTarget:mock_tab_groups_handler_
                             forProtocol:@protocol(TabGroupsCommands)];
    [dispatcher startDispatchingToTarget:mock_tab_grid_handler_
                             forProtocol:@protocol(TabGridCommands)];
  }

  // Returns a vector containing the 3 distant tabs.
  std::vector<SavedTabGroupTab> CreateSavedTabs(base::Uuid saved_tab_group_id) {
    std::vector<SavedTabGroupTab> tabs;
    tabs.push_back(FirstTab(saved_tab_group_id));
    tabs.push_back(SecondTab(saved_tab_group_id));
    tabs.push_back(ThirdTab(saved_tab_group_id));
    return tabs;
  }

  // Verify that the `web_state_list` contains the 3 local tabs.
  // `web_state_offset` is used to provide the number of web_states that were
  // added before the tabs.
  void VerifyLocalTabGroup(WebStateList* web_state_list, int web_state_offset) {
    web::WebState* first_web_state =
        web_state_list->GetWebStateAt(0 + web_state_offset);
    EXPECT_EQ(kFirstTabURL, first_web_state->GetVisibleURL());
    EXPECT_EQ(kFirstTabTitle, first_web_state->GetTitle());

    web::WebState* second_web_state =
        web_state_list->GetWebStateAt(1 + web_state_offset);
    EXPECT_EQ(kSecondTabURL, second_web_state->GetVisibleURL());
    EXPECT_EQ(kSecondTabTitle, second_web_state->GetTitle());

    web::WebState* third_web_state =
        web_state_list->GetWebStateAt(2 + web_state_offset);
    EXPECT_EQ(kThirdTabURL, third_web_state->GetVisibleURL());
    EXPECT_EQ(kThirdTabTitle, third_web_state->GetTitle());
  }

  // Return a distant tab at position 0 with the "first" ids.
  SavedTabGroupTab FirstTab(base::Uuid group_guid) {
    return SavedTabGroupTab(kFirstTabURL, kFirstTabTitle, group_guid,
                            std::make_optional(0), kFirstTabId);
  }

  // Return a distant tab at position 1 with the "second" ids.
  SavedTabGroupTab SecondTab(base::Uuid group_guid) {
    return SavedTabGroupTab(kSecondTabURL, kSecondTabTitle, group_guid,
                            std::make_optional(1), kSecondTabId);
  }

  // Return a distant tab at position 2 with the "third" ids.
  SavedTabGroupTab ThirdTab(base::Uuid group_guid) {
    return SavedTabGroupTab(kThirdTabURL, kThirdTabTitle, group_guid,
                            std::make_optional(2), kThirdTabId);
  }

  // Returns the sync tab group prediction for the given `saved_group`.
  auto SyncTabGroupPrediction(SavedTabGroup saved_group) {
    return AllOf(
        Property(&SavedTabGroup::local_group_id, saved_group.local_group_id()),
        Property(&SavedTabGroup::title, saved_group.title()),
        Property(&SavedTabGroup::color, saved_group.color()));
  }

  // Creates a vector of `saved_tabs` based on the given `range`.
  std::vector<SavedTabGroupTab> SavedTabGroupTabsFromTabs(
      std::vector<int> indexes,
      WebStateList* web_state_list,
      base::Uuid saved_tab_group_id) {
    std::vector<SavedTabGroupTab> saved_tabs;
    for (int index : indexes) {
      web::WebState* web_state = web_state_list->GetWebStateAt(index);
      SavedTabGroupTab saved_tab(web_state->GetVisibleURL(),
                                 web_state->GetTitle(), saved_tab_group_id,
                                 std::make_optional(index), std::nullopt,
                                 web_state->GetUniqueIdentifier().identifier());
      saved_tabs.push_back(saved_tab);
    }
    return saved_tabs;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  id app_state_;
  FakeSceneState* scene_state_;
  FakeSceneState* scene_state_same_profile_;
  FakeSceneState* other_scene_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<Browser> browser_;
  raw_ptr<Browser> browser_same_profile_;
  std::unique_ptr<TestProfileIOS> other_profile_;
  raw_ptr<Browser> other_browser_;
  raw_ptr<Browser> other_inactive_browser_;
  raw_ptr<BrowserList> browser_list_;
  std::unique_ptr<IOSTabGroupSyncDelegate> delegate_;
  raw_ptr<MockTabGroupSyncService> mock_service_;
  const base::Uuid kFirstTabId = base::Uuid::GenerateRandomV4();
  const base::Uuid kSecondTabId = base::Uuid::GenerateRandomV4();
  const base::Uuid kThirdTabId = base::Uuid::GenerateRandomV4();
  const GURL kFirstTabURL = GURL("https://first_tab.com");
  const GURL kSecondTabURL = GURL("https://second_tab.com");
  const GURL kThirdTabURL = GURL("https://third_tab.com");
  const std::u16string kFirstTabTitle = u"first tab";
  const std::u16string kSecondTabTitle = u"second tab";
  const std::u16string kThirdTabTitle = u"third tab";
  const std::u16string kGroupTitle = u"my group title";
  const TabGroupColorId kGroupColor = TabGroupColorId::kPurple;

  id mock_application_handler_;
  id mock_tab_groups_handler_;
  id mock_tab_grid_handler_;
};

// Tests adding a tab group when the currently foregrounded active scene is with
// the same profile.
TEST_F(IOSTabGroupSyncDelegateTest, CreateTabGroupSameBrowserStateForeground) {
  scene_state_same_profile_.activationLevel =
      SceneActivationLevelForegroundActive;
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  other_scene_state_.activationLevel = SceneActivationLevelForegroundInactive;

  base::Uuid saved_tab_group_id = base::Uuid::GenerateRandomV4();

  EXPECT_CALL(*mock_service_,
              UpdateLocalTabGroupMapping(saved_tab_group_id, _,
                                         OpeningSource::kAutoOpenedFromSync));

  EXPECT_CALL(*mock_service_, UpdateLocalTabId(_, kFirstTabId, _));
  EXPECT_CALL(*mock_service_, UpdateLocalTabId(_, kSecondTabId, _));
  EXPECT_CALL(*mock_service_, UpdateLocalTabId(_, kThirdTabId, _));

  const GURL kFakeUrl = GURL("https://fakeWebState.com");
  std::unique_ptr<web::FakeWebState> fake_web_state =
      std::make_unique<web::FakeWebState>(web::WebStateID::NewUnique());
  fake_web_state->SetCurrentURL(kFakeUrl);
  WebStateList* target_web_state_list =
      browser_same_profile_->GetWebStateList();
  target_web_state_list->InsertWebState(std::move(fake_web_state));
  target_web_state_list->ActivateWebStateAt(0);

  SavedTabGroup saved_group(kGroupTitle, kGroupColor,
                            CreateSavedTabs(saved_tab_group_id),
                            std::make_optional(0), saved_tab_group_id);
  delegate_->CreateLocalTabGroup(saved_group);

  ASSERT_EQ(4, target_web_state_list->count());
  EXPECT_EQ(kFakeUrl, target_web_state_list->GetWebStateAt(0)->GetVisibleURL());
  EXPECT_EQ(0, target_web_state_list->active_index());
  EXPECT_FALSE(target_web_state_list->GetGroupOfWebStateAt(0));
  const TabGroup* tab_group = target_web_state_list->GetGroupOfWebStateAt(1);
  ASSERT_TRUE(tab_group);

  VerifyLocalTabGroup(target_web_state_list, 1);
}

// Tests adding a tab group when the currently foreground active scene is from
// another profile.
TEST_F(IOSTabGroupSyncDelegateTest, CreateTabGroupOtherBrowserStateForeground) {
  other_scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  scene_state_same_profile_.activationLevel = SceneActivationLevelBackground;

  base::Uuid saved_tab_group_id = base::Uuid::GenerateRandomV4();

  EXPECT_CALL(*mock_service_,
              UpdateLocalTabGroupMapping(saved_tab_group_id, _, _));

  EXPECT_CALL(*mock_service_, UpdateLocalTabId(_, kFirstTabId, _));
  EXPECT_CALL(*mock_service_, UpdateLocalTabId(_, kSecondTabId, _));
  EXPECT_CALL(*mock_service_, UpdateLocalTabId(_, kThirdTabId, _));

  const GURL kFakeUrl = GURL("https://fakeWebState.com");
  std::unique_ptr<web::FakeWebState> fake_web_state =
      std::make_unique<web::FakeWebState>(web::WebStateID::NewUnique());
  fake_web_state->SetCurrentURL(kFakeUrl);
  WebStateList* target_web_state_list = browser_->GetWebStateList();
  target_web_state_list->InsertWebState(std::move(fake_web_state));
  target_web_state_list->ActivateWebStateAt(0);

  SavedTabGroup saved_group(kGroupTitle, kGroupColor,
                            CreateSavedTabs(saved_tab_group_id),
                            std::make_optional(0), saved_tab_group_id);
  delegate_->CreateLocalTabGroup(saved_group);

  ASSERT_EQ(4, target_web_state_list->count());
  EXPECT_EQ(kFakeUrl, target_web_state_list->GetWebStateAt(0)->GetVisibleURL());
  EXPECT_EQ(0, target_web_state_list->active_index());
  EXPECT_FALSE(target_web_state_list->GetGroupOfWebStateAt(0));
  const TabGroup* tab_group = target_web_state_list->GetGroupOfWebStateAt(1);
  ASSERT_TRUE(tab_group);

  VerifyLocalTabGroup(target_web_state_list, 1);
}

// Tests adding a tab group when there is no currently foreground active scene,
// the only foreground scene is from another profile and there is one
// scene in background.
TEST_F(IOSTabGroupSyncDelegateTest, CreateTabGroupBackgroundScene) {
  other_scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  scene_state_.activationLevel = SceneActivationLevelBackground;
  scene_state_same_profile_.activationLevel = SceneActivationLevelDisconnected;

  base::Uuid saved_tab_group_id = base::Uuid::GenerateRandomV4();

  EXPECT_CALL(*mock_service_,
              UpdateLocalTabGroupMapping(saved_tab_group_id, _,
                                         OpeningSource::kAutoOpenedFromSync));

  EXPECT_CALL(*mock_service_, UpdateLocalTabId(_, kFirstTabId, _));
  EXPECT_CALL(*mock_service_, UpdateLocalTabId(_, kSecondTabId, _));
  EXPECT_CALL(*mock_service_, UpdateLocalTabId(_, kThirdTabId, _));

  const GURL kFakeUrl = GURL("https://fakeWebState.com");
  std::unique_ptr<web::FakeWebState> fake_web_state =
      std::make_unique<web::FakeWebState>(web::WebStateID::NewUnique());
  fake_web_state->SetCurrentURL(kFakeUrl);
  WebStateList* target_web_state_list = browser_->GetWebStateList();
  target_web_state_list->InsertWebState(std::move(fake_web_state));
  target_web_state_list->ActivateWebStateAt(0);

  SavedTabGroup saved_group(kGroupTitle, kGroupColor,
                            CreateSavedTabs(saved_tab_group_id),
                            std::make_optional(0), saved_tab_group_id);
  delegate_->CreateLocalTabGroup(saved_group);

  ASSERT_EQ(4, target_web_state_list->count());
  EXPECT_EQ(kFakeUrl, target_web_state_list->GetWebStateAt(0)->GetVisibleURL());
  EXPECT_EQ(0, target_web_state_list->active_index());
  EXPECT_FALSE(target_web_state_list->GetGroupOfWebStateAt(0));
  const TabGroup* tab_group = target_web_state_list->GetGroupOfWebStateAt(1);
  ASSERT_TRUE(tab_group);

  VerifyLocalTabGroup(target_web_state_list, 1);
}

// Tests `CloseLocalTabGroup`.
TEST_F(IOSTabGroupSyncDelegateTest, CloseLocalTabGroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a [0 b* c ] d",
                                                       browser_->GetProfile()));

  WebStateList* web_state_list_same_profile =
      browser_same_profile_->GetWebStateList();
  WebStateListBuilderFromDescription builder_same_profile(
      web_state_list_same_profile);
  ASSERT_TRUE(builder_same_profile.BuildWebStateListFromDescription(
      "| [1 e* f ] g h ", browser_->GetProfile()));

  LocalTabGroupID local_id_group_0 =
      builder.GetTabGroupForIdentifier('0')->tab_group_id();
  delegate_->CloseLocalTabGroup(local_id_group_0);
  EXPECT_EQ("| a d*", builder.GetWebStateListDescription());

  LocalTabGroupID local_id_group_1 =
      builder_same_profile.GetTabGroupForIdentifier('1')->tab_group_id();
  delegate_->CloseLocalTabGroup(local_id_group_1);
  EXPECT_EQ("| g* h", builder_same_profile.GetWebStateListDescription());
}

// Tests `CloseLocalTabGroup` correctly updates all local tabs.
TEST_F(IOSTabGroupSyncDelegateTest, UpdateLocalTabGroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* [0 b c ] d",
                                                       browser_->GetProfile()));

  WebStateList* web_state_list_same_profile =
      browser_same_profile_->GetWebStateList();
  WebStateListBuilderFromDescription builder_same_profile(
      web_state_list_same_profile);
  ASSERT_TRUE(builder_same_profile.BuildWebStateListFromDescription(
      "| [1 e* f ] g h ", browser_->GetProfile()));

  const TabGroup* tab_group = builder.GetTabGroupForIdentifier('0');
  base::Uuid saved_tab_group_id = base::Uuid::GenerateRandomV4();
  std::vector<SavedTabGroupTab> tabs;
  SavedTabGroupTab first_tab = FirstTab(saved_tab_group_id);
  SavedTabGroupTab second_tab = SecondTab(saved_tab_group_id);
  SavedTabGroupTab third_tab = ThirdTab(saved_tab_group_id);

  SavedTabGroup saved_group(u"my group", TabGroupColorId::kPink, tabs,
                            std::make_optional(0), saved_tab_group_id);
  saved_group.SetLocalGroupId(tab_group->tab_group_id());

  EXPECT_CALL(*mock_service_, UpdateLocalTabId(_, kFirstTabId, _)).Times(1);
  EXPECT_CALL(*mock_service_, UpdateLocalTabId(_, kSecondTabId, _)).Times(2);
  EXPECT_CALL(*mock_service_, UpdateLocalTabId(_, kThirdTabId, _)).Times(2);

  // Update the local group with only one tab.
  saved_group.AddTabFromSync(first_tab);
  delegate_->UpdateLocalTabGroup(saved_group);
  web::WebState* first_web_state = web_state_list->GetWebStateAt(1);
  FakeUpdateLocalTabId(first_web_state, first_tab, saved_group);
  ASSERT_EQ(3, web_state_list->count());
  EXPECT_FALSE(first_web_state->IsRealized());
  EXPECT_EQ(kFirstTabURL, first_web_state->GetVisibleURL());
  EXPECT_EQ(kFirstTabTitle, first_web_state->GetTitle());
  EXPECT_EQ(1, tab_group->range().count());
  EXPECT_NSEQ(tab_group->GetTitle(), @"my group");
  EXPECT_TRUE([tab_group->GetColor()
      isEqual:TabGroup::ColorForTabGroupColorId(TabGroupColorId::kPink)]);

  // Update the local group by adding 2 new tabs.
  saved_group.AddTabFromSync(second_tab);
  saved_group.AddTabFromSync(third_tab);
  delegate_->UpdateLocalTabGroup(saved_group);
  first_web_state = web_state_list->GetWebStateAt(1);
  web::WebState* second_web_state = web_state_list->GetWebStateAt(2);
  web::WebState* third_web_state = web_state_list->GetWebStateAt(3);
  FakeUpdateLocalTabId(second_web_state, second_tab, saved_group);
  FakeUpdateLocalTabId(third_web_state, third_tab, saved_group);
  ASSERT_EQ(5, web_state_list->count());
  EXPECT_FALSE(first_web_state->IsRealized());
  EXPECT_FALSE(second_web_state->IsRealized());
  EXPECT_FALSE(third_web_state->IsRealized());
  EXPECT_EQ(kFirstTabURL, first_web_state->GetVisibleURL());
  EXPECT_EQ(kFirstTabTitle, first_web_state->GetTitle());
  EXPECT_EQ(kSecondTabURL, second_web_state->GetVisibleURL());
  EXPECT_EQ(kSecondTabTitle, second_web_state->GetTitle());
  EXPECT_EQ(kThirdTabURL, third_web_state->GetVisibleURL());
  EXPECT_EQ(kThirdTabTitle, third_web_state->GetTitle());
  EXPECT_EQ(3, tab_group->range().count());

  // Move the second tab at the end and remove the first tab.
  saved_group.MoveTabLocally(kSecondTabId, 2);
  saved_group.RemoveTabFromSync(kFirstTabId);
  delegate_->UpdateLocalTabGroup(saved_group);
  second_web_state = web_state_list->GetWebStateAt(2);
  third_web_state = web_state_list->GetWebStateAt(1);
  ASSERT_EQ(4, web_state_list->count());
  EXPECT_FALSE(second_web_state->IsRealized());
  EXPECT_FALSE(third_web_state->IsRealized());
  EXPECT_EQ(kSecondTabURL, second_web_state->GetVisibleURL());
  EXPECT_EQ(kSecondTabTitle, second_web_state->GetTitle());
  EXPECT_EQ(kThirdTabURL, third_web_state->GetVisibleURL());
  EXPECT_EQ(kThirdTabTitle, third_web_state->GetTitle());
  EXPECT_EQ(2, tab_group->range().count());

  // Update the URL of both tabs.
  GURL second_tab_updated_url = GURL("https://second_tab_updated.com");
  GURL third_tab_updated_url = GURL("https://third_tab_updated.com");
  second_tab.SetURL(second_tab_updated_url);
  third_tab.SetURL(third_tab_updated_url);
  saved_group.UpdateTab(second_tab);
  saved_group.UpdateTab(third_tab);
  delegate_->UpdateLocalTabGroup(saved_group);
  second_web_state = web_state_list->GetWebStateAt(2);
  third_web_state = web_state_list->GetWebStateAt(1);
  FakeUpdateLocalTabId(second_web_state, second_tab, saved_group);
  FakeUpdateLocalTabId(third_web_state, third_tab, saved_group);
  ASSERT_EQ(4, web_state_list->count());
  EXPECT_FALSE(second_web_state->IsRealized());
  EXPECT_FALSE(third_web_state->IsRealized());
  EXPECT_EQ(second_tab_updated_url, second_web_state->GetVisibleURL());
  EXPECT_EQ(kSecondTabTitle, second_web_state->GetTitle());
  EXPECT_EQ(third_tab_updated_url, third_web_state->GetVisibleURL());
  EXPECT_EQ(kThirdTabTitle, third_web_state->GetTitle());
  EXPECT_EQ(2, tab_group->range().count());
}

// Tests `CloseLocalTabGroup` correctly updates local group with one tab.
TEST_F(IOSTabGroupSyncDelegateTest, UpdateLocalTabGroupOneTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* [0 b ] c",
                                                       browser_->GetProfile()));

  WebStateList* web_state_list_same_profile =
      browser_same_profile_->GetWebStateList();
  WebStateListBuilderFromDescription builder_same_profile(
      web_state_list_same_profile);
  ASSERT_TRUE(builder_same_profile.BuildWebStateListFromDescription(
      "| [1 e* f ] g h ", browser_->GetProfile()));

  const TabGroup* tab_group = builder.GetTabGroupForIdentifier('0');
  base::Uuid saved_tab_group_id = base::Uuid::GenerateRandomV4();
  std::vector<SavedTabGroupTab> tabs;
  SavedTabGroupTab first_tab = FirstTab(saved_tab_group_id);

  SavedTabGroup saved_group(u"my group", TabGroupColorId::kPink, tabs,
                            std::make_optional(0), saved_tab_group_id);
  saved_group.SetLocalGroupId(tab_group->tab_group_id());

  EXPECT_CALL(*mock_service_, UpdateLocalTabId(_, kFirstTabId, _)).Times(1);

  // Update the local group with only one tab.
  saved_group.AddTabFromSync(first_tab);
  delegate_->UpdateLocalTabGroup(saved_group);
  web::WebState* first_web_state = web_state_list->GetWebStateAt(1);
  FakeUpdateLocalTabId(first_web_state, first_tab, saved_group);
  ASSERT_EQ(3, web_state_list->count());
  EXPECT_FALSE(first_web_state->IsRealized());
  EXPECT_EQ(kFirstTabURL, first_web_state->GetVisibleURL());
  EXPECT_EQ(kFirstTabTitle, first_web_state->GetTitle());
  EXPECT_EQ(1, tab_group->range().count());
  EXPECT_NSEQ(tab_group->GetTitle(), @"my group");
  EXPECT_TRUE([tab_group->GetColor()
      isEqual:TabGroup::ColorForTabGroupColorId(TabGroupColorId::kPink)]);
}

// Tests that the service correctly returns local ids.
TEST_F(IOSTabGroupSyncDelegateTest, GetLocalTabGroupIds) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a [0 b* c ] d [1 e ]",
                                                       browser_->GetProfile()));
  auto local_group_ids = delegate_->GetLocalTabGroupIds();
  EXPECT_EQ(2u, local_group_ids.size());
}

// Tests that the service correctly returns local ids.
TEST_F(IOSTabGroupSyncDelegateTest, GetLocalTabIdsForTabGroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a [0 b* c ] d [1 e ]",
                                                       browser_->GetProfile()));

  LocalTabGroupID local_id_group_0 =
      builder.GetTabGroupForIdentifier('0')->tab_group_id();
  auto local_tab_ids = delegate_->GetLocalTabIdsForTabGroup(local_id_group_0);
  EXPECT_EQ(2u, local_tab_ids.size());

  LocalTabGroupID local_id_group_2 = TabGroupId::GenerateNew();
  local_tab_ids = delegate_->GetLocalTabIdsForTabGroup(local_id_group_2);
  EXPECT_EQ(0u, local_tab_ids.size());
}

// Tests that the service is correctly updated when creating a remote tab group.
TEST_F(IOSTabGroupSyncDelegateTest, CreateRemoteTabGroup) {
  WebStateList* web_state_list = browser_same_profile_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a b c* d e f"));

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  base::Uuid saved_tab_group_id = base::Uuid::GenerateRandomV4();

  tab_groups::TabGroupVisualData visual_data(
      kGroupTitle, tab_groups::TabGroupColorId::kBlue);
  web_state_list->CreateGroup({0, 1}, visual_data, tab_group_id);

  std::vector<SavedTabGroupTab> saved_tabs =
      SavedTabGroupTabsFromTabs({0, 1}, web_state_list, saved_tab_group_id);
  SavedTabGroup saved_group(kGroupTitle, visual_data.color(), saved_tabs,
                            std::nullopt, saved_tab_group_id, tab_group_id);

  EXPECT_CALL(*mock_service_, GetGroup(tab_group_id));
  EXPECT_CALL(*mock_service_, AddGroup(SyncTabGroupPrediction(saved_group)));
  delegate_->CreateRemoteTabGroup(tab_group_id);
}

// Tests opening an unknown tab group ID doesn't do anything.
TEST_F(IOSTabGroupSyncDelegateTest,
       HandleOpenTabGroupRequest_UnknownSavedTabGroupID) {
  base::Uuid saved_tab_group_id = base::Uuid::GenerateRandomV4();

  EXPECT_CALL(*mock_service_, GetGroup(saved_tab_group_id))
      .WillOnce(Return(std::nullopt));
  delegate_->HandleOpenTabGroupRequest(
      saved_tab_group_id, std::make_unique<IOSTabGroupActionContext>(browser_));

  // Check that no tab group was opened locally.
  auto local_group_ids = delegate_->GetLocalTabGroupIds();
  EXPECT_EQ(0u, local_group_ids.size());
}

// Tests opening a tab group from sync that isn't already open locally.
TEST_F(IOSTabGroupSyncDelegateTest,
       HandleOpenTabGroupRequest_UnopenedSavedTabGroup) {
  // Have another scene as the active one to make sure that the `browser` passed
  // is correctly used.
  scene_state_same_profile_.activationLevel =
      SceneActivationLevelForegroundActive;
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;

  base::Uuid saved_tab_group_id = base::Uuid::GenerateRandomV4();
  SavedTabGroup saved_group(kGroupTitle, kGroupColor,
                            CreateSavedTabs(saved_tab_group_id),
                            std::make_optional(0), saved_tab_group_id);

  __block const TabGroup* tab_group_shown;
  __block const TabGroup* tab_group_for_grid;
  __block BOOL grid_updated = NO;
  OCMStub([mock_application_handler_
      displayTabGridInMode:TabGridOpeningMode::kRegular]);

  OCMStub([mock_tab_groups_handler_
              showTabGroup:(const TabGroup*)[OCMArg anyPointer]])
      .andDo(^(NSInvocation* invocation) {
        [invocation getArgument:&tab_group_shown atIndex:2];
      });

  OCMStub([mock_tab_grid_handler_
              bringGroupIntoView:(const TabGroup*)[OCMArg anyPointer]
                        animated:NO])
      .andDo(^(NSInvocation* invocation) {
        grid_updated = YES;
        [invocation getArgument:&tab_group_for_grid atIndex:2];
      });
  EXPECT_CALL(*mock_service_, GetGroup(saved_tab_group_id))
      .WillOnce(Return(saved_group));
  delegate_->HandleOpenTabGroupRequest(
      saved_tab_group_id, std::make_unique<IOSTabGroupActionContext>(browser_));

  // Check that a tab group was opened locally.
  auto local_group_ids = delegate_->GetLocalTabGroupIds();
  EXPECT_EQ(1u, local_group_ids.size());
  const auto local_group_id = local_group_ids[0];
  const auto local_tab_group_info =
      tab_groups::utils::GetLocalTabGroupInfo(browser_list_, local_group_id);
  EXPECT_EQ(browser_->GetWebStateList(), local_tab_group_info.web_state_list);
  const auto groups = local_tab_group_info.web_state_list->GetGroups();
  EXPECT_EQ(1u, groups.size());
  EXPECT_OCMOCK_VERIFY(mock_application_handler_);
  EXPECT_OCMOCK_VERIFY(mock_tab_groups_handler_);
  // The grid operation is happening with a delay.
  EXPECT_TRUE(
      base::test::ios::WaitUntilConditionOrTimeout(base::Milliseconds(300), ^{
        return grid_updated;
      }));
  EXPECT_OCMOCK_VERIFY(mock_tab_grid_handler_);
  EXPECT_EQ(tab_group_shown, tab_group_for_grid);
  EXPECT_TRUE(groups.contains(tab_group_shown));
  EXPECT_TRUE(groups.contains(tab_group_for_grid));
}

// Tests opening a tab group from sync that is already open locally in this
// window doesn't open a new local group.
TEST_F(IOSTabGroupSyncDelegateTest,
       HandleOpenTabGroupRequest_OpenedSavedTabGroupSameWindow) {
  // Create a local group.
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a [0 b* c ] d",
                                                       browser_->GetProfile()));
  const TabGroup* local_group = builder.GetTabGroupForIdentifier('0');
  LocalTabGroupID local_id_group_0 = local_group->tab_group_id();
  ASSERT_EQ(1u, delegate_->GetLocalTabGroupIds().size());
  ASSERT_EQ(1u, web_state_list->GetGroups().size());
  // Create the associated distant group.
  base::Uuid saved_tab_group_id = base::Uuid::GenerateRandomV4();
  SavedTabGroup saved_group(kGroupTitle, kGroupColor,
                            CreateSavedTabs(saved_tab_group_id),
                            std::make_optional(0), saved_tab_group_id);
  saved_group.SetLocalGroupId(local_id_group_0);

  OCMStub([mock_application_handler_
      displayTabGridInMode:TabGridOpeningMode::kRegular]);

  OCMStub([mock_tab_groups_handler_ showTabGroup:local_group]);

  __block BOOL grid_updated = NO;
  OCMStub([mock_tab_grid_handler_ bringGroupIntoView:local_group animated:NO])
      .andDo(^(NSInvocation* invocation) {
        grid_updated = YES;
      });
  EXPECT_CALL(*mock_service_, GetGroup(saved_tab_group_id))
      .WillOnce(Return(saved_group));
  delegate_->HandleOpenTabGroupRequest(
      saved_tab_group_id, std::make_unique<IOSTabGroupActionContext>(browser_));

  // Check that there is still only one tab group opened locally.
  auto local_group_ids = delegate_->GetLocalTabGroupIds();
  EXPECT_EQ(1u, local_group_ids.size());
  EXPECT_EQ(1u, web_state_list->GetGroups().size());
  EXPECT_OCMOCK_VERIFY(mock_application_handler_);
  EXPECT_OCMOCK_VERIFY(mock_tab_groups_handler_);
  // The grid operation is happening with a delay.
  EXPECT_TRUE(
      base::test::ios::WaitUntilConditionOrTimeout(base::Milliseconds(300), ^{
        return grid_updated;
      }));
  EXPECT_OCMOCK_VERIFY(mock_tab_grid_handler_);
}

// Tests opening a tab group from sync that is already open locally on another
// window doesn't open a new local group.
TEST_F(IOSTabGroupSyncDelegateTest,
       HandleOpenTabGroupRequest_OpenedSavedTabGroupDifferentWindow) {
  // Create a local group.
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a [0 b* c ] d",
                                                       browser_->GetProfile()));
  const TabGroup* local_tab_group = builder.GetTabGroupForIdentifier('0');
  LocalTabGroupID local_id_group_0 = local_tab_group->tab_group_id();
  ASSERT_EQ(1u, delegate_->GetLocalTabGroupIds().size());
  ASSERT_EQ(1u, web_state_list->GetGroups().size());
  // Create the associated distant group.
  base::Uuid saved_tab_group_id = base::Uuid::GenerateRandomV4();
  SavedTabGroup saved_group(kGroupTitle, kGroupColor,
                            CreateSavedTabs(saved_tab_group_id),
                            std::make_optional(0), saved_tab_group_id);
  saved_group.SetLocalGroupId(local_id_group_0);

  SceneState* scene_state = browser_->GetSceneState();
  scene_state.UIEnabled = NO;
  UIApplication* app = [UIApplication sharedApplication];
  id app_mock = OCMPartialMock(app);
  if (@available(iOS 17, *)) {
    id request_arg =
        [OCMArg checkWithBlock:^BOOL(UISceneSessionActivationRequest* request) {
          return request.session == scene_state.scene.session &&
                 request.options.requestingScene ==
                     browser_same_profile_->GetSceneState().scene;
        }];
    OCMStub([app_mock activateSceneSessionForRequest:request_arg
                                        errorHandler:[OCMArg any]])
        .andDo(^(NSInvocation* invocation) {
          scene_state.UIEnabled = YES;
        });
  } else {
    OCMStub([app_mock requestSceneSessionActivation:scene_state.scene.session
                                       userActivity:nil
                                            options:[OCMArg any]
                                       errorHandler:[OCMArg any]])
        .andDo(^(NSInvocation* invocation) {
          scene_state.UIEnabled = YES;
        });
  }

  OCMStub([mock_application_handler_
      displayTabGridInMode:TabGridOpeningMode::kRegular]);
  OCMStub([mock_tab_groups_handler_ showTabGroup:local_tab_group]);

  EXPECT_CALL(*mock_service_, GetGroup(saved_tab_group_id))
      .WillOnce(Return(saved_group));
  delegate_->HandleOpenTabGroupRequest(
      saved_tab_group_id,
      std::make_unique<IOSTabGroupActionContext>(browser_same_profile_));

  // Check that there is still only one tab group opened locally.
  auto local_group_ids = delegate_->GetLocalTabGroupIds();
  EXPECT_EQ(1u, local_group_ids.size());
  EXPECT_EQ(1u, web_state_list->GetGroups().size());
  EXPECT_OCMOCK_VERIFY(mock_application_handler_);
  EXPECT_OCMOCK_VERIFY(mock_tab_groups_handler_);
}

}  // namespace tab_groups
