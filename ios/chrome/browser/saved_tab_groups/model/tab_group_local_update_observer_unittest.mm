// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/tab_group_local_update_observer.h"

#import <memory>
#import <optional>
#import <string>

#import "base/memory/raw_ptr.h"
#import "base/uuid.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/saved_tab_group_tab.h"
#import "components/saved_tab_groups/public/types.h"
#import "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#import "components/tab_groups/tab_group_color.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/sessions/model/test_session_restoration_service.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using testing::_;

using ::testing::AllOf;
using ::testing::Property;
using ::testing::Return;

namespace tab_groups {

namespace {

const char kTestURL[] = "https://chromium.org";

std::unique_ptr<KeyedService> CreateMockSyncService(
    web::BrowserState* context) {
  return std::make_unique<MockTabGroupSyncService>();
}

// Returns the sync tab group prediction for the given `saved_group`.
auto SyncTabGroupPrediction(SavedTabGroup saved_group) {
  return AllOf(
      Property(&SavedTabGroup::local_group_id, saved_group.local_group_id()),
      Property(&SavedTabGroup::title, saved_group.title()),
      Property(&SavedTabGroup::color, saved_group.color()));
}

// Returns a test `SavedTabGroup`.
SavedTabGroup TestSavedGroup() {
  SavedTabGroup saved_group(u"Test title", tab_groups::TabGroupColorId::kBlue,
                            {}, std::nullopt, base::Uuid::GenerateRandomV4(),
                            TabGroupId::GenerateNew());
  return saved_group;
}

MATCHER_P(TabTitleEq, title, "") {
  return arg.title() == title;
}

MATCHER_P(TabURLEq, url, "") {
  return arg.url() == url;
}

}  // namespace

class TabGroupLocalUpdateObserverTest : public PlatformTest {
 public:
  TabGroupLocalUpdateObserverTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(TabGroupSyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        SessionRestorationServiceFactory::GetInstance(),
        TestSessionRestorationService::GetTestingFactory());
    profile_ = std::move(builder).Build();

    mock_service_ = static_cast<MockTabGroupSyncService*>(
        TabGroupSyncServiceFactory::GetForProfile(profile_.get()));

    browser_ = std::make_unique<TestBrowser>(
        profile_.get(), nil, std::make_unique<FakeWebStateListDelegate>());
    browser_same_profile_ = std::make_unique<TestBrowser>(
        profile_.get(), nil, std::make_unique<FakeWebStateListDelegate>());

    other_profile_ = TestProfileIOS::Builder().Build();
    other_browser_ = std::make_unique<TestBrowser>(
        other_profile_.get(), nil,
        std::make_unique<FakeWebStateListDelegate>());

    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());
    local_observer_ = std::make_unique<TabGroupLocalUpdateObserver>(
        browser_list_.get(), mock_service_);
    browser_list_->AddBrowser(browser_.get());

    BrowserList* other_browser_list =
        BrowserListFactory::GetForProfile(other_profile_.get());
    other_browser_list->AddBrowser(other_browser_.get());
  }

  // Returns a new fake web state.
  std::unique_ptr<web::FakeWebState> CreateWebState() {
    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>(web::WebStateID::NewUnique());
    web_state->SetCurrentURL(GURL("http://first-url.com"));
    web_state->SetTitle(u"original title");
    return web_state;
  }

  // Inserts a new FakeWebState in `web_state_list`.
  web::FakeWebState* InsertWebState(WebStateList* web_state_list) {
    std::unique_ptr<web::FakeWebState> unique_web_state = CreateWebState();
    web::FakeWebState* web_state = unique_web_state.get();
    web_state_list->InsertWebState(std::move(unique_web_state));
    return web_state;
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

  void SetUpNavigationContext(web::FakeWebState* web_state) {
    navigation_item_ = web::NavigationItem::Create();
    navigation_item_->SetTransitionType(ui::PAGE_TRANSITION_TYPED);

    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->SetLastCommittedItem(navigation_item_.get());

    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetBrowserState(profile_.get());

    navigation_context_ = std::make_unique<web::FakeNavigationContext>();
    navigation_context_->SetWebState(web_state);

    navigation_context_->SetUrl(GURL(kTestURL));
    navigation_context_->SetHasCommitted(true);
    navigation_context_->SetPageTransition(ui::PAGE_TRANSITION_LINK);
    navigation_context_->SetHasUserGesture(true);
    navigation_context_->SetIsRendererInitiated(false);
    navigation_context_->SetIsPost(false);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestBrowser> browser_same_profile_;
  std::unique_ptr<TestProfileIOS> other_profile_;
  std::unique_ptr<TestBrowser> other_browser_;
  raw_ptr<BrowserList> browser_list_;
  std::unique_ptr<TabGroupLocalUpdateObserver> local_observer_;
  raw_ptr<MockTabGroupSyncService> mock_service_;
  const std::u16string kNewTitle = u"title to update";
  std::unique_ptr<web::NavigationItem> navigation_item_;
  std::unique_ptr<web::FakeNavigationContext> navigation_context_;
};

// Tests that the service is correctly updated when the title of a tab that was
// added after creating the service is updated.
TEST_F(TabGroupLocalUpdateObserverTest, TitleUpdateExistingTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();

  web::FakeWebState* web_state = InsertWebState(web_state_list);
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  web_state_list->CreateGroup({0}, {}, tab_group_id);

  EXPECT_CALL(*mock_service_, UpdateTab(tab_group_id, web_state_id.identifier(),
                                        TabTitleEq(kNewTitle)))
      .Times(1);
  web_state->SetTitle(kNewTitle);
}

// Tests that the service is correctly updated when the title of a tab that was
// existing when creating the service is updated.
TEST_F(TabGroupLocalUpdateObserverTest, TitleUpdateNewTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();

  web::FakeWebState* web_state = InsertWebState(web_state_list);
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  web_state_list->CreateGroup({0}, {}, tab_group_id);

  EXPECT_CALL(*mock_service_, UpdateTab(tab_group_id, web_state_id.identifier(),
                                        TabTitleEq(kNewTitle)));
  web_state->SetTitle(kNewTitle);
}

// Tests that the service is does not update the title when sync is paused.
TEST_F(TabGroupLocalUpdateObserverTest, TitleUpdateNewTabSyncPaused) {
  local_observer_->SetSyncUpdatePaused(true);

  WebStateList* web_state_list = browser_->GetWebStateList();

  web::FakeWebState* web_state = InsertWebState(web_state_list);
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  web_state_list->CreateGroup({0}, {}, tab_group_id);

  EXPECT_CALL(*mock_service_, UpdateTab(tab_group_id, web_state_id.identifier(),
                                        TabTitleEq(kNewTitle)))
      .Times(0);
  web_state->SetTitle(kNewTitle);
}

// Tests that the service is correctly updated when the title of a tab that is
// in a WebStateList that was added after the service creation is updated.
TEST_F(TabGroupLocalUpdateObserverTest, TitleUpdateNewWebStateList) {
  WebStateList* web_state_list = browser_same_profile_->GetWebStateList();

  web::FakeWebState* web_state = InsertWebState(web_state_list);
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  web_state_list->CreateGroup({0}, {}, tab_group_id);

  // Add the Browser after the tab is inserted.
  BrowserListFactory::GetForProfile(profile_.get())
      ->AddBrowser(browser_same_profile_.get());

  EXPECT_CALL(*mock_service_, UpdateTab(tab_group_id, web_state_id.identifier(),
                                        TabTitleEq(kNewTitle)));
  web_state->SetTitle(kNewTitle);
}

// Tests that the service is correctly updated when the title of a tab that
// inserted in a WebStateList that was added after the service creation is
// updated.
TEST_F(TabGroupLocalUpdateObserverTest, TitleUpdateNewWebStateListInsert) {
  // Add the browser before inserting the tab.
  BrowserListFactory::GetForProfile(profile_.get())
      ->AddBrowser(browser_same_profile_.get());

  WebStateList* web_state_list = browser_same_profile_->GetWebStateList();

  std::unique_ptr<web::FakeWebState> unique_web_state = CreateWebState();
  web::WebStateID web_state_id = unique_web_state->GetUniqueIdentifier();
  web::FakeWebState* web_state = unique_web_state.get();
  web_state_list->InsertWebState(std::move(unique_web_state));

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  web_state_list->CreateGroup({0}, {}, tab_group_id);

  EXPECT_CALL(*mock_service_, UpdateTab(tab_group_id, web_state_id.identifier(),
                                        TabTitleEq(kNewTitle)));
  web_state->SetTitle(kNewTitle);
}

// Tests that the service is correctly updated when the navigation of a tab is
// updated.
TEST_F(TabGroupLocalUpdateObserverTest, NavigationUpdate) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  web::FakeWebState* web_state = InsertWebState(web_state_list);
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  web_state_list->CreateGroup({0}, {}, tab_group_id);

  EXPECT_CALL(*mock_service_, UpdateTab(tab_group_id, web_state_id.identifier(),
                                        TabURLEq(GURL(kTestURL))));
  web_state->SetCurrentURL(GURL(kTestURL));
  SetUpNavigationContext(web_state);
  web_state->OnNavigationFinished(navigation_context_.get());
}

// Tests that the service is not updated when the new active tab is not in the
// group.
TEST_F(TabGroupLocalUpdateObserverTest, ActivateRegularTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  InsertWebState(web_state_list);
  InsertWebState(web_state_list);

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  web_state_list->CreateGroup({0}, {}, tab_group_id);
  web_state_list->ActivateWebStateAt(0);

  EXPECT_CALL(*mock_service_, UpdateTab(_, _, _)).Times(0);
  EXPECT_CALL(*mock_service_, AddTab(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*mock_service_, RemoveTab(_, _)).Times(0);
  EXPECT_CALL(*mock_service_, MoveTab(_, _, _)).Times(0);

  web_state_list->ActivateWebStateAt(1);
}

// Tests that the service is not updated when sync is paused and the navigation
// of a tab is updated.
TEST_F(TabGroupLocalUpdateObserverTest, NavigationUpdateSyncPaused) {
  local_observer_->SetSyncUpdatePaused(true);

  WebStateList* web_state_list = browser_->GetWebStateList();
  web::FakeWebState* web_state = InsertWebState(web_state_list);
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  web_state_list->CreateGroup({0}, {}, tab_group_id);

  EXPECT_CALL(*mock_service_, UpdateTab(tab_group_id, web_state_id.identifier(),
                                        TabURLEq(GURL(kTestURL))))
      .Times(0);
  web_state->SetCurrentURL(GURL(kTestURL));
  SetUpNavigationContext(web_state);
  web_state->OnNavigationFinished(navigation_context_.get());
}

// Tests that the service is correctly updated when a tab is added to a newly
// created group.
TEST_F(TabGroupLocalUpdateObserverTest, AddTabToNewGroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  web::FakeWebState* web_state = InsertWebState(web_state_list);
  web_state->SetCurrentURL(GURL(kTestURL));
  web_state->SetTitle(kNewTitle);

  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  TabGroupId tab_group_id = TabGroupId::GenerateNew();

  EXPECT_CALL(*mock_service_,
              AddTab(tab_group_id, web_state_id.identifier(), kNewTitle,
                     GURL(kTestURL), std::make_optional(0ul)));
  web_state_list->CreateGroup({0}, {}, tab_group_id);
}

// Tests that the service is not updated when sync is paused and a tab is added
// to a group.
TEST_F(TabGroupLocalUpdateObserverTest, AddTabSyncPaused) {
  local_observer_->SetSyncUpdatePaused(true);

  WebStateList* web_state_list = browser_->GetWebStateList();
  web::FakeWebState* web_state = InsertWebState(web_state_list);
  web_state->SetCurrentURL(GURL(kTestURL));
  web_state->SetTitle(kNewTitle);

  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  TabGroupId tab_group_id = TabGroupId::GenerateNew();

  EXPECT_CALL(*mock_service_,
              AddTab(tab_group_id, web_state_id.identifier(), kNewTitle,
                     GURL(kTestURL), std::make_optional(0ul)))
      .Times(0);
  web_state_list->CreateGroup({0}, {}, tab_group_id);
}

// Tests that the service is correctly updated when tabs are moved in and out of
// a group.
TEST_F(TabGroupLocalUpdateObserverTest, MoveTabToGroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  web::FakeWebState* web_state_1 = InsertWebState(web_state_list);
  web::FakeWebState* web_state_2 = InsertWebState(web_state_list);
  web::FakeWebState* web_state_3 = InsertWebState(web_state_list);

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  web_state_3->SetCurrentURL(GURL(kTestURL));
  web_state_3->SetTitle(kNewTitle);
  web::WebStateID web_state_1_id = web_state_1->GetUniqueIdentifier();
  web::WebStateID web_state_2_id = web_state_2->GetUniqueIdentifier();
  web::WebStateID web_state_3_id = web_state_3->GetUniqueIdentifier();

  // Create the group and insert `web_state_1`.
  EXPECT_CALL(*mock_service_, AddTab(tab_group_id, web_state_1_id.identifier(),
                                     _, _, std::make_optional(0ul)));
  const TabGroup* group = web_state_list->CreateGroup({0}, {}, tab_group_id);
  EXPECT_CALL(*mock_service_, GetGroup(tab_group_id))
      .WillOnce(Return(TestSavedGroup()));

  // Move `web_state_2` in the group.
  EXPECT_CALL(*mock_service_, AddTab(tab_group_id, web_state_2_id.identifier(),
                                     _, _, std::make_optional(1ul)));
  web_state_list->MoveToGroup({1}, group);

  // Move `web_state_3` in the group.
  EXPECT_CALL(*mock_service_,
              AddTab(tab_group_id, web_state_3_id.identifier(), kNewTitle,
                     GURL(kTestURL), std::make_optional(2ul)));
  web_state_list->MoveToGroup({2}, group);

  // Move `web_state_3` at the beginning of the group.
  EXPECT_CALL(*mock_service_,
              MoveTab(tab_group_id, web_state_3_id.identifier(), 0));
  web_state_list->MoveWebStateAt(2, 0);

  // Move `web_state_3` out of the group.
  EXPECT_CALL(*mock_service_,
              RemoveTab(tab_group_id, web_state_3_id.identifier()));
  web_state_list->RemoveFromGroups({0});
}

// Tests that the service is correctly updated when tabs are moved between
// groups.
TEST_F(TabGroupLocalUpdateObserverTest, MoveTabFromOtherGroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  web::FakeWebState* web_state_1 = InsertWebState(web_state_list);
  web::FakeWebState* web_state_2 = InsertWebState(web_state_list);
  web::FakeWebState* web_state_3 = InsertWebState(web_state_list);

  TabGroupId tab_group_1_id = TabGroupId::GenerateNew();
  TabGroupId tab_group_2_id = TabGroupId::GenerateNew();

  web::WebStateID web_state_1_id = web_state_1->GetUniqueIdentifier();
  web::WebStateID web_state_2_id = web_state_2->GetUniqueIdentifier();
  web::WebStateID web_state_3_id = web_state_3->GetUniqueIdentifier();

  // Create a first group and insert `web_state_1`.
  EXPECT_CALL(*mock_service_,
              AddTab(tab_group_1_id, web_state_1_id.identifier(), _, _,
                     std::make_optional(0ul)));
  const TabGroup* group_1 =
      web_state_list->CreateGroup({0}, {}, tab_group_1_id);

  // Create a second group and insert `web_state_2`.
  EXPECT_CALL(*mock_service_,
              AddTab(tab_group_2_id, web_state_2_id.identifier(), _, _,
                     std::make_optional(0ul)));
  const TabGroup* group_2 =
      web_state_list->CreateGroup({1}, {}, tab_group_2_id);
  EXPECT_CALL(*mock_service_, GetGroup(tab_group_2_id))
      .WillRepeatedly(Return(TestSavedGroup()));

  // Move `web_state_3` in the second group.
  EXPECT_CALL(*mock_service_,
              AddTab(tab_group_2_id, web_state_3_id.identifier(), _, _,
                     std::make_optional(1ul)));
  web_state_list->MoveToGroup({2}, group_2);

  // Move `web_state_2` in the first group.
  EXPECT_CALL(*mock_service_,
              RemoveTab(tab_group_2_id, web_state_2_id.identifier()));
  EXPECT_CALL(*mock_service_,
              AddTab(tab_group_1_id, web_state_2_id.identifier(), _, _,
                     std::make_optional(1ul)));
  web_state_list->MoveToGroup({1}, group_1);
}

// Tests that the service is correctly updated when a tab is removed from a
// group.
TEST_F(TabGroupLocalUpdateObserverTest, RemoveFromGroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  web::FakeWebState* web_state = InsertWebState(web_state_list);
  InsertWebState(web_state_list);

  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  TabGroupId tab_group_id = TabGroupId::GenerateNew();

  web_state_list->CreateGroup({0, 1}, {}, tab_group_id);
  EXPECT_CALL(*mock_service_, GetGroup(tab_group_id))
      .WillOnce(Return(TestSavedGroup()));

  EXPECT_CALL(*mock_service_,
              RemoveTab(tab_group_id, web_state_id.identifier()));
  web_state_list->CloseWebStateAt(/*index*/ 0, WebStateList::CLOSE_NO_FLAGS);
}

// Tests that the service is not updated when sync is pausded and a tab is
// removed from a group.
TEST_F(TabGroupLocalUpdateObserverTest, RemoveFromGroupSyncPaused) {
  local_observer_->SetSyncUpdatePaused(true);

  WebStateList* web_state_list = browser_->GetWebStateList();
  web::FakeWebState* web_state = InsertWebState(web_state_list);
  InsertWebState(web_state_list);

  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  TabGroupId tab_group_id = TabGroupId::GenerateNew();

  web_state_list->CreateGroup({0, 1}, {}, tab_group_id);
  EXPECT_CALL(*mock_service_, GetGroup(tab_group_id)).Times(0);

  EXPECT_CALL(*mock_service_,
              RemoveTab(tab_group_id, web_state_id.identifier()))
      .Times(0);
  web_state_list->CloseWebStateAt(/*index*/ 0, WebStateList::CLOSE_NO_FLAGS);
}

// Tests that the service is correctly updated when a raw tab group is created.
TEST_F(TabGroupLocalUpdateObserverTest, CreateRawSyncedGroup) {
  WebStateList* web_state_list = browser_same_profile_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a b c* d e f"));

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  base::Uuid saved_tab_group_id = base::Uuid::GenerateRandomV4();

  std::vector<SavedTabGroupTab> saved_tabs =
      SavedTabGroupTabsFromTabs({0}, web_state_list, saved_tab_group_id);
  SavedTabGroup saved_group(
      u"", TabGroup::DefaultColorForNewTabGroup(web_state_list), saved_tabs,
      std::nullopt, saved_tab_group_id, tab_group_id);

  BrowserListFactory::GetForProfile(profile_.get())
      ->AddBrowser(browser_same_profile_.get());

  EXPECT_CALL(*mock_service_, GetGroup(tab_group_id));
  EXPECT_CALL(*mock_service_, AddGroup(SyncTabGroupPrediction(saved_group)));
  web_state_list->CreateGroup({0}, {}, tab_group_id);
}

// Tests that the service is correctly updated when a tab group is created.
TEST_F(TabGroupLocalUpdateObserverTest, CreateNamedSyncedGroup) {
  WebStateList* web_state_list = browser_same_profile_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a b] c* d e f"));

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  base::Uuid saved_tab_group_id = base::Uuid::GenerateRandomV4();

  std::vector<SavedTabGroupTab> saved_tabs =
      SavedTabGroupTabsFromTabs({2, 3, 4}, web_state_list, saved_tab_group_id);
  SavedTabGroup saved_group(u"Test title", tab_groups::TabGroupColorId::kBlue,
                            saved_tabs, std::nullopt, saved_tab_group_id,
                            tab_group_id);

  BrowserListFactory::GetForProfile(profile_.get())
      ->AddBrowser(browser_same_profile_.get());

  EXPECT_CALL(*mock_service_, GetGroup(tab_group_id));
  EXPECT_CALL(*mock_service_, AddGroup(SyncTabGroupPrediction(saved_group)));
  tab_groups::TabGroupVisualData visual_data(
      u"Test title", tab_groups::TabGroupColorId::kBlue);
  web_state_list->CreateGroup({2, 3, 4}, visual_data, tab_group_id);
}

// Tests that the service is not updated when sync is paused.
TEST_F(TabGroupLocalUpdateObserverTest, CreateSyncedGroupSyncPaused) {
  local_observer_->SetSyncUpdatePaused(true);

  WebStateList* web_state_list = browser_same_profile_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a b c* d e f"));

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  base::Uuid saved_tab_group_id = base::Uuid::GenerateRandomV4();

  std::vector<SavedTabGroupTab> saved_tabs =
      SavedTabGroupTabsFromTabs({0}, web_state_list, saved_tab_group_id);
  SavedTabGroup saved_group(
      u"", TabGroup::DefaultColorForNewTabGroup(web_state_list), saved_tabs,
      std::nullopt, saved_tab_group_id, tab_group_id);

  BrowserListFactory::GetForProfile(profile_.get())
      ->AddBrowser(browser_same_profile_.get());

  EXPECT_CALL(*mock_service_, GetGroup(tab_group_id));
  EXPECT_CALL(*mock_service_, AddGroup(SyncTabGroupPrediction(saved_group)))
      .Times(0);
  web_state_list->CreateGroup({0}, {}, tab_group_id);
}

// Tests that the service is correctly updated when the visual data of a group
// is updated.
TEST_F(TabGroupLocalUpdateObserverTest, UpdateVisualData) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  InsertWebState(web_state_list);
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  const TabGroup* group = web_state_list->CreateGroup({0}, {}, tab_group_id);
  TabGroupVisualData visual_data(u"Updated Group Name",
                                 tab_groups::TabGroupColorId::kRed);

  EXPECT_CALL(*mock_service_,
              UpdateVisualData(tab_group_id, &group->visual_data()));
  web_state_list->UpdateGroupVisualData(group, visual_data);
}

// Tests that the service is correctly updated when the visual data of a group
// is updated.
TEST_F(TabGroupLocalUpdateObserverTest, UpdateVisualDataSyncPaused) {
  local_observer_->SetSyncUpdatePaused(true);

  WebStateList* web_state_list = browser_->GetWebStateList();
  InsertWebState(web_state_list);
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  const TabGroup* group = web_state_list->CreateGroup({0}, {}, tab_group_id);
  TabGroupVisualData visual_data(u"Updated Group Name",
                                 tab_groups::TabGroupColorId::kRed);

  EXPECT_CALL(*mock_service_,
              UpdateVisualData(tab_group_id, &group->visual_data()))
      .Times(0);
  web_state_list->UpdateGroupVisualData(group, visual_data);
}

// Tests that the service is correctly updated when a tab is replaced in a
// group.
TEST_F(TabGroupLocalUpdateObserverTest, ReplaceTabInGroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a b] c* d e f"));

  web::WebState* web_state_b = builder.GetWebStateForIdentifier('b');
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');
  auto passed_web_state = std::make_unique<web::FakeWebState>();

  web::WebStateID web_state_b_id = web_state_b->GetUniqueIdentifier();
  web::WebStateID passed_web_state_id = passed_web_state->GetUniqueIdentifier();
  TabGroupId tab_group_id = group->tab_group_id();
  EXPECT_CALL(*mock_service_, GetGroup(tab_group_id))
      .WillOnce(Return(TestSavedGroup()));

  EXPECT_CALL(*mock_service_,
              RemoveTab(tab_group_id, web_state_b_id.identifier()));
  EXPECT_CALL(*mock_service_,
              AddTab(tab_group_id, passed_web_state_id.identifier(), _, _,
                     std::make_optional(1ul)));
  web_state_list->ReplaceWebStateAt(/*index=*/1, std::move(passed_web_state));
}

// Tests that the service is not updated when sync is paused and a tab is
// replaced in a group.
TEST_F(TabGroupLocalUpdateObserverTest, ReplaceTabInGroupSyncPaused) {
  local_observer_->SetSyncUpdatePaused(true);

  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a b] c* d e f"));

  web::WebState* web_state_b = builder.GetWebStateForIdentifier('b');
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');
  auto passed_web_state = std::make_unique<web::FakeWebState>();

  web::WebStateID web_state_b_id = web_state_b->GetUniqueIdentifier();
  web::WebStateID passed_web_state_id = passed_web_state->GetUniqueIdentifier();
  TabGroupId tab_group_id = group->tab_group_id();
  EXPECT_CALL(*mock_service_, GetGroup(tab_group_id)).Times(0);

  EXPECT_CALL(*mock_service_,
              RemoveTab(tab_group_id, web_state_b_id.identifier()))
      .Times(0);
  EXPECT_CALL(*mock_service_,
              AddTab(tab_group_id, passed_web_state_id.identifier(), _, _,
                     std::make_optional(1ul)))
      .Times(0);
  web_state_list->ReplaceWebStateAt(/*index=*/1, std::move(passed_web_state));
}

// Tests that the service is correctly updated when a tab is inserted in a
// group.
TEST_F(TabGroupLocalUpdateObserverTest, InsertInGroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a b] c* d e f"));

  const TabGroup* group = builder.GetTabGroupForIdentifier('0');
  auto passed_web_state = std::make_unique<web::FakeWebState>();

  web::WebStateID passed_web_state_id = passed_web_state->GetUniqueIdentifier();
  TabGroupId tab_group_id = group->tab_group_id();

  EXPECT_CALL(*mock_service_,
              AddTab(tab_group_id, passed_web_state_id.identifier(), _, _,
                     std::make_optional(1ul)));
  web_state_list->InsertWebState(std::move(passed_web_state),
                                 WebStateList::InsertionParams::AtIndex(1));
}

// Tests that the service is not updated when sync is pauded and a tab is
// inserted in a group.
TEST_F(TabGroupLocalUpdateObserverTest, InsertInGroupSyncPaused) {
  local_observer_->SetSyncUpdatePaused(true);

  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a b] c* d e f"));

  const TabGroup* group = builder.GetTabGroupForIdentifier('0');
  auto passed_web_state = std::make_unique<web::FakeWebState>();

  web::WebStateID passed_web_state_id = passed_web_state->GetUniqueIdentifier();
  TabGroupId tab_group_id = group->tab_group_id();

  EXPECT_CALL(*mock_service_,
              AddTab(tab_group_id, passed_web_state_id.identifier(), _, _,
                     std::make_optional(1ul)))
      .Times(0);
  web_state_list->InsertWebState(std::move(passed_web_state),
                                 WebStateList::InsertionParams::AtIndex(1));
}

// Tests that the service is correctly updated when a tab group is deleted.
TEST_F(TabGroupLocalUpdateObserverTest, DeleteGroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  InsertWebState(web_state_list);
  InsertWebState(web_state_list);

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  const TabGroup* group = web_state_list->CreateGroup({0, 1}, {}, tab_group_id);
  EXPECT_CALL(*mock_service_, GetGroup(tab_group_id))
      .WillRepeatedly(Return(TestSavedGroup()));

  EXPECT_CALL(*mock_service_, RemoveLocalTabGroupMapping(tab_group_id, _))
      .Times(0);
  EXPECT_CALL(*mock_service_, RemoveGroup(tab_group_id));
  web_state_list->DeleteGroup(group);
}

// Tests that the service is correctly updated when a tab group is closed
// locally.
TEST_F(TabGroupLocalUpdateObserverTest, CloseGroupLocally) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  InsertWebState(web_state_list);
  InsertWebState(web_state_list);
  InsertWebState(web_state_list);
  InsertWebState(web_state_list);

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  const TabGroup* group = web_state_list->CreateGroup({0, 1}, {}, tab_group_id);
  EXPECT_EQ(1u, web_state_list->GetGroups().size());
  std::optional<SavedTabGroup> saved_group = TestSavedGroup();

  EXPECT_CALL(*mock_service_, GetGroup(tab_group_id))
      .WillRepeatedly([&saved_group] { return saved_group; });
  EXPECT_CALL(*mock_service_, RemoveLocalTabGroupMapping(tab_group_id, _))
      .WillOnce([&saved_group] { saved_group = std::nullopt; });
  EXPECT_CALL(*mock_service_, RemoveGroup(tab_group_id)).Times(0);
  EXPECT_CALL(*mock_service_, RemoveTab(tab_group_id, _)).Times(0);
  utils::CloseTabGroupLocally(group, web_state_list, mock_service_);

  EXPECT_EQ(0u, web_state_list->GetGroups().size());
  EXPECT_EQ(2, web_state_list->count());

  web_state_list->MoveWebStateAt(0, 1);
  web_state_list->CloseWebStateAt(/*index*/ 0, WebStateList::CLOSE_NO_FLAGS);
  EXPECT_EQ(1, web_state_list->count());
}

// Tests that the service is correctly updated when the last tab of a group is
// deleted.
TEST_F(TabGroupLocalUpdateObserverTest, DeleteGroupAfterRemovingLastTtab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  web::FakeWebState* web_state = InsertWebState(web_state_list);
  InsertWebState(web_state_list);

  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  TabGroupId tab_group_id = TabGroupId::GenerateNew();

  web_state_list->CreateGroup({0}, {}, tab_group_id);
  EXPECT_CALL(*mock_service_, GetGroup(tab_group_id))
      .WillRepeatedly(Return(TestSavedGroup()));

  EXPECT_CALL(*mock_service_,
              RemoveTab(tab_group_id, web_state_id.identifier()));
  EXPECT_CALL(*mock_service_, RemoveLocalTabGroupMapping(tab_group_id, _))
      .Times(0);
  EXPECT_CALL(*mock_service_, RemoveGroup(tab_group_id));
  web_state_list->CloseWebStateAt(/*index*/ 0, WebStateList::CLOSE_NO_FLAGS);
}

// Tests that the service is correctly called when the active tab is updated.
TEST_F(TabGroupLocalUpdateObserverTest, UpdateActiveTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a b] c* d e f"));

  const TabGroup* group = builder.GetTabGroupForIdentifier('0');
  web::WebState* web_state_a = builder.GetWebStateForIdentifier('a');

  EXPECT_CALL(*mock_service_,
              OnTabSelected(group->tab_group_id(),
                            web_state_a->GetUniqueIdentifier().identifier()));
  web_state_list->ActivateWebStateAt(0);
}

}  // namespace tab_groups
