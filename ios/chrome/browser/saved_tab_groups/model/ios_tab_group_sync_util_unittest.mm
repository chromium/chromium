// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"

#import "base/test/scoped_feature_list.h"
#import "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_local_update_observer.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace tab_groups {
namespace utils {

namespace {

// Creates a MockTabGroupSyncService.
std::unique_ptr<KeyedService> CreateMockSyncService(
    web::BrowserState* context) {
  return std::make_unique<::testing::NiceMock<MockTabGroupSyncService>>();
}

// Returns the tab ID for the web state at `index` in `browser`.
web::WebStateID GetTabIDForWebStateAt(int index, Browser* browser) {
  web::WebState* web_state = browser->GetWebStateList()->GetWebStateAt(index);
  return web_state->GetUniqueIdentifier();
}

}  // namespace

class TabGroupSyncUtilTest : public PlatformTest {
 protected:
  TabGroupSyncUtilTest() {
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        TabGroupSyncServiceFactory::GetInstance(),
        base::BindRepeating(&CreateMockSyncService));
    profile_ = std::move(test_profile_builder).Build();

    mock_service_ = static_cast<MockTabGroupSyncService*>(
        TabGroupSyncServiceFactory::GetForProfile(profile_.get()));

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    other_browser_ = std::make_unique<TestBrowser>(profile_.get());

    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());
    local_observer_ = std::make_unique<TabGroupLocalUpdateObserver>(
        browser_list_.get(), mock_service_);

    browser_list_->AddBrowser(browser_.get());
    browser_list_->AddBrowser(other_browser_.get());

    SnapshotBrowserAgent::CreateForBrowser(browser_.get());
    SnapshotBrowserAgent::CreateForBrowser(other_browser_.get());
  }

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {kTabGroupsIPad, kModernTabStrip, kTabGroupSync}, {});
    AppendNewWebState(browser_.get());
    AppendNewWebState(browser_.get());
    AppendNewWebState(browser_.get());
  }

  // Appends a new web state in the web state list of `browser`.
  web::FakeWebState* AppendNewWebState(Browser* browser) {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    web::FakeWebState* inserted_web_state = fake_web_state.get();
    SnapshotTabHelper::CreateForWebState(inserted_web_state);
    browser->GetWebStateList()->InsertWebState(
        std::move(fake_web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    return inserted_web_state;
  }

  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<Browser> other_browser_;
  raw_ptr<BrowserList> browser_list_;
  raw_ptr<MockTabGroupSyncService> mock_service_;
  std::unique_ptr<TabGroupLocalUpdateObserver> local_observer_;
};

// Tests that a tab group with one tab is moved from one regular browser to
// another browser.
TEST_F(TabGroupSyncUtilTest, TestMoveTabGroupOneTabAcrossRegularBrowsers) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateList* other_web_state_list = other_browser_->GetWebStateList();

  // Create a group of two tabs.
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  TabGroupVisualData visual_data =
      TabGroupVisualData(u"Group", TabGroupColorId::kGrey);
  const TabGroup* tab_group = web_state_list->CreateGroup(
      {2}, TabGroupVisualData(visual_data), tab_group_id);

  web::WebStateID tab_id = GetTabIDForWebStateAt(2, browser_.get());
  ASSERT_EQ(3, web_state_list->count());
  ASSERT_EQ(tab_group, web_state_list->GetGroupOfWebStateAt(2));

  EXPECT_CALL(*mock_service_, CreateScopedLocalObserverPauser()).Times(1);
  local_observer_->SetSyncUpdatePaused(true);
  EXPECT_CALL(*mock_service_, RemoveGroup(tab_group_id)).Times(0);

  // Move the group.
  MoveTabGroupToBrowser(tab_group, other_browser_.get(), 0);

  const TabGroup* other_group = other_web_state_list->GetGroupOfWebStateAt(0);
  ASSERT_TRUE(other_group);
  EXPECT_EQ(1, other_group->range().count());
  EXPECT_EQ(tab_group_id, other_group->tab_group_id());
  EXPECT_EQ(visual_data, other_group->visual_data());
  EXPECT_EQ(2, web_state_list->count());
  EXPECT_EQ(1, other_web_state_list->count());
  EXPECT_NE(tab_id, GetTabIDForWebStateAt(1, browser_.get()));
  EXPECT_EQ(tab_id, GetTabIDForWebStateAt(0, other_browser_.get()));
}

// Tests that a tab group with multiple tabs is moved from one regular browser
// to another browser.
TEST_F(TabGroupSyncUtilTest, TestMoveTabGroupMutipleTabsAcrossRegularBrowsers) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateList* other_web_state_list = other_browser_->GetWebStateList();

  // Create a group of two tabs.
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  TabGroupVisualData visual_data =
      TabGroupVisualData(u"Group", TabGroupColorId::kGrey);
  const TabGroup* tab_group = web_state_list->CreateGroup(
      {0, 1}, TabGroupVisualData(visual_data), tab_group_id);
  web::WebStateID tab_id_0 = GetTabIDForWebStateAt(0, browser_.get());
  web::WebStateID tab_id_1 = GetTabIDForWebStateAt(1, browser_.get());
  ASSERT_EQ(3, web_state_list->count());
  ASSERT_EQ(tab_group, web_state_list->GetGroupOfWebStateAt(0));

  EXPECT_CALL(*mock_service_, CreateScopedLocalObserverPauser()).Times(1);
  local_observer_->SetSyncUpdatePaused(true);
  EXPECT_CALL(*mock_service_, RemoveGroup(tab_group_id)).Times(0);

  // Move the group.
  MoveTabGroupToBrowser(tab_group, other_browser_.get(), 0);

  const TabGroup* other_group = other_web_state_list->GetGroupOfWebStateAt(0);
  ASSERT_TRUE(other_group);
  EXPECT_EQ(2, other_group->range().count());
  EXPECT_EQ(tab_group_id, other_group->tab_group_id());
  EXPECT_EQ(visual_data, other_group->visual_data());
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(2, other_web_state_list->count());
  EXPECT_EQ(tab_id_0, GetTabIDForWebStateAt(0, other_browser_.get()));
  EXPECT_EQ(tab_id_1, GetTabIDForWebStateAt(1, other_browser_.get()));
}

// Tests that a tab group with multiple tabs is moved from one regular browser
// to another browser.
TEST_F(TabGroupSyncUtilTest, TestMoveTabGroupsAcrossRegularBrowsers) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateList* other_web_state_list = other_browser_->GetWebStateList();

  // Create 2 groups.
  TabGroupId tab_group_id_0 = TabGroupId::GenerateNew();
  TabGroupId tab_group_id_1 = TabGroupId::GenerateNew();
  TabGroupVisualData visual_data =
      TabGroupVisualData(u"Group", TabGroupColorId::kGrey);
  const TabGroup* tab_group_0 = web_state_list->CreateGroup(
      {0}, TabGroupVisualData(visual_data), tab_group_id_0);
  const TabGroup* tab_group_1 = web_state_list->CreateGroup(
      {1}, TabGroupVisualData(visual_data), tab_group_id_1);
  web::WebStateID tab_id_0 = GetTabIDForWebStateAt(0, browser_.get());
  web::WebStateID tab_id_1 = GetTabIDForWebStateAt(1, browser_.get());
  ASSERT_EQ(3, web_state_list->count());
  ASSERT_EQ(tab_group_0, web_state_list->GetGroupOfWebStateAt(0));
  ASSERT_EQ(tab_group_1, web_state_list->GetGroupOfWebStateAt(1));

  EXPECT_CALL(*mock_service_, CreateScopedLocalObserverPauser()).Times(2);
  local_observer_->SetSyncUpdatePaused(true);
  EXPECT_CALL(*mock_service_, RemoveGroup(tab_group_id_0)).Times(0);
  EXPECT_CALL(*mock_service_, RemoveGroup(tab_group_id_1)).Times(0);

  // Move groups.
  MoveTabGroupToBrowser(tab_group_0, other_browser_.get(), 0);
  MoveTabGroupToBrowser(tab_group_1, other_browser_.get(), 1);

  const TabGroup* other_group_0 = other_web_state_list->GetGroupOfWebStateAt(0);
  const TabGroup* other_group_1 = other_web_state_list->GetGroupOfWebStateAt(1);
  ASSERT_TRUE(other_group_0);
  ASSERT_TRUE(other_group_1);
  EXPECT_EQ(1, other_group_0->range().count());
  EXPECT_EQ(1, other_group_1->range().count());
  EXPECT_EQ(tab_group_id_0, other_group_0->tab_group_id());
  EXPECT_EQ(tab_group_id_1, other_group_1->tab_group_id());
  EXPECT_EQ(visual_data, other_group_0->visual_data());
  EXPECT_EQ(visual_data, other_group_1->visual_data());
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(2, other_web_state_list->count());
  EXPECT_EQ(tab_id_0, GetTabIDForWebStateAt(0, other_browser_.get()));
  EXPECT_EQ(tab_id_1, GetTabIDForWebStateAt(1, other_browser_.get()));
}

// Tests that a tab group with multiple tabs is moved from one regular browser
// to another browser with tab group sync disabled.
TEST_F(TabGroupSyncUtilTest,
       TestMoveTabGroupsAcrossRegularBrowsersSyncDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(kTabGroupSync);

  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateList* other_web_state_list = other_browser_->GetWebStateList();

  // Create 2 groups.
  TabGroupId tab_group_id_0 = TabGroupId::GenerateNew();
  TabGroupId tab_group_id_1 = TabGroupId::GenerateNew();
  TabGroupVisualData visual_data =
      TabGroupVisualData(u"Group", TabGroupColorId::kGrey);
  const TabGroup* tab_group_0 = web_state_list->CreateGroup(
      {0}, TabGroupVisualData(visual_data), tab_group_id_0);
  const TabGroup* tab_group_1 = web_state_list->CreateGroup(
      {1}, TabGroupVisualData(visual_data), tab_group_id_1);
  web::WebStateID tab_id_0 = GetTabIDForWebStateAt(0, browser_.get());
  web::WebStateID tab_id_1 = GetTabIDForWebStateAt(1, browser_.get());
  ASSERT_EQ(3, web_state_list->count());
  ASSERT_EQ(tab_group_0, web_state_list->GetGroupOfWebStateAt(0));
  ASSERT_EQ(tab_group_1, web_state_list->GetGroupOfWebStateAt(1));

  EXPECT_CALL(*mock_service_, CreateScopedLocalObserverPauser()).Times(0);
  EXPECT_CALL(*mock_service_, RemoveGroup(tab_group_id_0)).Times(0);
  EXPECT_CALL(*mock_service_, RemoveGroup(tab_group_id_1)).Times(0);

  // Move groups.
  MoveTabGroupToBrowser(tab_group_0, other_browser_.get(), 0);
  MoveTabGroupToBrowser(tab_group_1, other_browser_.get(), 1);

  const TabGroup* other_group_0 = other_web_state_list->GetGroupOfWebStateAt(0);
  const TabGroup* other_group_1 = other_web_state_list->GetGroupOfWebStateAt(1);
  ASSERT_TRUE(other_group_0);
  ASSERT_TRUE(other_group_1);
  EXPECT_EQ(1, other_group_0->range().count());
  EXPECT_EQ(1, other_group_1->range().count());
  EXPECT_EQ(tab_group_id_0, other_group_0->tab_group_id());
  EXPECT_EQ(tab_group_id_1, other_group_1->tab_group_id());
  EXPECT_EQ(visual_data, other_group_0->visual_data());
  EXPECT_EQ(visual_data, other_group_1->visual_data());
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(2, other_web_state_list->count());
  EXPECT_EQ(tab_id_0, GetTabIDForWebStateAt(0, other_browser_.get()));
  EXPECT_EQ(tab_id_1, GetTabIDForWebStateAt(1, other_browser_.get()));
}

// Tests that moving a tab group to its owning browser with the same destination
// index is a no-op.
TEST_F(TabGroupSyncUtilTest, MoveTabGroupToItsOwningBrowser_SameIndex) {
  WebStateList* web_state_list = browser_->GetWebStateList();

  // Create a group of two tabs.
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  TabGroupVisualData visual_data =
      TabGroupVisualData(u"Group", TabGroupColorId::kGrey);
  const TabGroup* tab_group = web_state_list->CreateGroup(
      {0, 1}, TabGroupVisualData(visual_data), tab_group_id);
  web::WebStateID tab_id_0 = GetTabIDForWebStateAt(0, browser_.get());
  web::WebStateID tab_id_1 = GetTabIDForWebStateAt(1, browser_.get());
  ASSERT_EQ(3, web_state_list->count());
  ASSERT_EQ(tab_group, web_state_list->GetGroupOfWebStateAt(0));

  EXPECT_CALL(*mock_service_, CreateScopedLocalObserverPauser()).Times(0);
  EXPECT_CALL(*mock_service_, RemoveGroup(tab_group_id)).Times(0);

  // Move the group.
  MoveTabGroupToBrowser(tab_group, browser_.get(), 0);

  EXPECT_EQ(tab_group, web_state_list->GetGroupOfWebStateAt(0));
  EXPECT_EQ(tab_group, web_state_list->GetGroupOfWebStateAt(1));
  EXPECT_EQ(nullptr, web_state_list->GetGroupOfWebStateAt(2));
  EXPECT_EQ(2, tab_group->range().count());
  EXPECT_EQ(tab_group_id, tab_group->tab_group_id());
  EXPECT_EQ(visual_data, tab_group->visual_data());
  EXPECT_EQ(3, web_state_list->count());
  EXPECT_EQ(tab_id_0, GetTabIDForWebStateAt(0, browser_.get()));
  EXPECT_EQ(tab_id_1, GetTabIDForWebStateAt(1, browser_.get()));
}

// Tests that moving a tab group to its owning browser to a position on its
// right moves the group accordingly.
TEST_F(TabGroupSyncUtilTest, MoveTabGroupToItsOwningBrowser_MovingRight) {
  WebStateList* web_state_list = browser_->GetWebStateList();

  // Create a group of two tabs.
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  TabGroupVisualData visual_data =
      TabGroupVisualData(u"Group", TabGroupColorId::kGrey);
  const TabGroup* tab_group = web_state_list->CreateGroup(
      {0, 1}, TabGroupVisualData(visual_data), tab_group_id);
  web::WebStateID tab_id_0 = GetTabIDForWebStateAt(0, browser_.get());
  web::WebStateID tab_id_1 = GetTabIDForWebStateAt(1, browser_.get());
  ASSERT_EQ(3, web_state_list->count());
  ASSERT_EQ(tab_group, web_state_list->GetGroupOfWebStateAt(0));

  EXPECT_CALL(*mock_service_, CreateScopedLocalObserverPauser()).Times(0);
  EXPECT_CALL(*mock_service_, RemoveGroup(tab_group_id)).Times(0);

  // Move the group.
  MoveTabGroupToBrowser(tab_group, browser_.get(), 3);

  EXPECT_EQ(nullptr, web_state_list->GetGroupOfWebStateAt(0));
  EXPECT_EQ(tab_group, web_state_list->GetGroupOfWebStateAt(1));
  EXPECT_EQ(tab_group, web_state_list->GetGroupOfWebStateAt(2));
  EXPECT_EQ(2, tab_group->range().count());
  EXPECT_EQ(tab_group_id, tab_group->tab_group_id());
  EXPECT_EQ(visual_data, tab_group->visual_data());
  EXPECT_EQ(3, web_state_list->count());
  EXPECT_EQ(tab_id_0, GetTabIDForWebStateAt(1, browser_.get()));
  EXPECT_EQ(tab_id_1, GetTabIDForWebStateAt(2, browser_.get()));
}

// Tests that moving a tab group to its owning browser to a position on its left
// moves the group accordingly.
TEST_F(TabGroupSyncUtilTest, MoveTabGroupToItsOwningBrowser_MovingLeft) {
  WebStateList* web_state_list = browser_->GetWebStateList();

  // Create a group of two tabs.
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  TabGroupVisualData visual_data =
      TabGroupVisualData(u"Group", TabGroupColorId::kGrey);
  const TabGroup* tab_group = web_state_list->CreateGroup(
      {1, 2}, TabGroupVisualData(visual_data), tab_group_id);
  web::WebStateID tab_id_1 = GetTabIDForWebStateAt(1, browser_.get());
  web::WebStateID tab_id_2 = GetTabIDForWebStateAt(2, browser_.get());
  ASSERT_EQ(3, web_state_list->count());
  ASSERT_EQ(nullptr, web_state_list->GetGroupOfWebStateAt(0));
  ASSERT_EQ(tab_group, web_state_list->GetGroupOfWebStateAt(1));
  ASSERT_EQ(tab_group, web_state_list->GetGroupOfWebStateAt(2));

  EXPECT_CALL(*mock_service_, CreateScopedLocalObserverPauser()).Times(0);
  EXPECT_CALL(*mock_service_, RemoveGroup(tab_group_id)).Times(0);

  // Move the group to the left.
  MoveTabGroupToBrowser(tab_group, browser_.get(), 0);

  EXPECT_EQ(tab_group, web_state_list->GetGroupOfWebStateAt(0));
  EXPECT_EQ(tab_group, web_state_list->GetGroupOfWebStateAt(1));
  EXPECT_EQ(2, tab_group->range().count());
  EXPECT_EQ(tab_group_id, tab_group->tab_group_id());
  EXPECT_EQ(visual_data, tab_group->visual_data());
  EXPECT_EQ(3, web_state_list->count());
  EXPECT_EQ(tab_id_1, GetTabIDForWebStateAt(0, browser_.get()));
  EXPECT_EQ(tab_id_2, GetTabIDForWebStateAt(1, browser_.get()));
}

TEST_F(TabGroupSyncUtilTest, ShouldUpdateHistory) {
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  item->SetTransitionType(ui::PAGE_TRANSITION_TYPED);

  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  navigation_manager->SetLastCommittedItem(item.get());

  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetNavigationManager(std::move(navigation_manager));

  auto profile = std::make_unique<web::FakeBrowserState>();
  profile->SetOffTheRecord(false);
  web_state->SetBrowserState(profile.get());

  web::FakeNavigationContext navigation;
  navigation.SetWebState(web_state.get());
  navigation.SetHasCommitted(true);

  EXPECT_TRUE(ShouldUpdateHistory(&navigation));

  // Off the record navigation.
  profile->SetOffTheRecord(true);
  EXPECT_FALSE(ShouldUpdateHistory(&navigation));
  profile->SetOffTheRecord(false);

  // Back / forward navigation.
  EXPECT_TRUE(ShouldUpdateHistory(&navigation));
  item->SetTransitionType(ui::PAGE_TRANSITION_FORWARD_BACK);
  EXPECT_FALSE(ShouldUpdateHistory(&navigation));
  item->SetTransitionType(ui::PAGE_TRANSITION_TYPED);

  // Navigation error.
  EXPECT_TRUE(ShouldUpdateHistory(&navigation));
  navigation.SetError([NSError
      errorWithDomain:@"commm"
                 code:200
             userInfo:@{@"Error reason" : @"Invalid Input"}]);
  EXPECT_FALSE(ShouldUpdateHistory(&navigation));
  item->SetTransitionType(ui::PAGE_TRANSITION_TYPED);
}

TEST_F(TabGroupSyncUtilTest, IsSaveableNavigation) {
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  item->SetTransitionType(ui::PAGE_TRANSITION_TYPED);

  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  navigation_manager->SetLastCommittedItem(item.get());

  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetNavigationManager(std::move(navigation_manager));

  auto profile = std::make_unique<web::FakeBrowserState>();
  profile->SetOffTheRecord(false);
  web_state->SetBrowserState(profile.get());

  web::FakeNavigationContext navigation;
  navigation.SetWebState(web_state.get());

  navigation.SetUrl(GURL("https://google.com"));
  navigation.SetHasCommitted(true);
  navigation.SetPageTransition(ui::PAGE_TRANSITION_LINK);
  navigation.SetHasUserGesture(true);
  navigation.SetIsRendererInitiated(false);
  navigation.SetIsPost(false);
  EXPECT_TRUE(IsSaveableNavigation(&navigation));

  // Invalid URL scheme.
  navigation.SetUrl(GURL("ftp://google.com"));
  EXPECT_FALSE(IsSaveableNavigation(&navigation));

  // URL is empty.
  navigation.SetUrl(GURL());
  EXPECT_FALSE(IsSaveableNavigation(&navigation));

  // URL is chrome:// scheme.
  navigation.SetUrl(GURL("chrome://flags"));
  EXPECT_FALSE(IsSaveableNavigation(&navigation));

  // URL is HTTP.
  navigation.SetUrl(GURL("http://example.com"));
  EXPECT_TRUE(IsSaveableNavigation(&navigation));

  // URL is NTP.
  navigation.SetUrl(GURL("chrome://newtab"));
  EXPECT_TRUE(IsSaveableNavigation(&navigation));

  // Non-committed navigation.
  EXPECT_TRUE(IsSaveableNavigation(&navigation));
  navigation.SetHasCommitted(false);
  EXPECT_FALSE(IsSaveableNavigation(&navigation));
  navigation.SetHasCommitted(true);

  // POST request.
  EXPECT_TRUE(IsSaveableNavigation(&navigation));
  navigation.SetIsPost(true);
  EXPECT_FALSE(IsSaveableNavigation(&navigation));
  navigation.SetIsPost(false);

  // Server redirect.
  EXPECT_TRUE(IsSaveableNavigation(&navigation));
  navigation.SetPageTransition(ui::PAGE_TRANSITION_SERVER_REDIRECT);
  EXPECT_FALSE(IsSaveableNavigation(&navigation));

  // Client redirect.
  navigation.SetPageTransition(ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  EXPECT_FALSE(IsSaveableNavigation(&navigation));
  navigation.SetPageTransition(ui::PAGE_TRANSITION_LINK);

  // Not a main frame navigation.
  EXPECT_TRUE(IsSaveableNavigation(&navigation));
  navigation.SetPageTransition(ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
  EXPECT_FALSE(IsSaveableNavigation(&navigation));
  navigation.SetPageTransition(ui::PAGE_TRANSITION_LINK);

  // Not user initiated navigation.
  EXPECT_TRUE(IsSaveableNavigation(&navigation));
  navigation.SetHasUserGesture(false);
  navigation.SetIsRendererInitiated(true);
  EXPECT_FALSE(IsSaveableNavigation(&navigation));

  // Browser initiated navigation.
  navigation.SetIsRendererInitiated(false);
  EXPECT_TRUE(IsSaveableNavigation(&navigation));

  // Off the record navigation.
  EXPECT_TRUE(IsSaveableNavigation(&navigation));
  profile->SetOffTheRecord(true);
  EXPECT_FALSE(IsSaveableNavigation(&navigation));
}

}  // namespace utils
}  // namespace tab_groups
