// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/tabs_closure_util.h"

#import "base/time/time.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/sessions/model/fake_tab_restore_service.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/sessions/model/test_session_restoration_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {
using tabs_closure_util::CloseTabs;
using tabs_closure_util::GetTabGroupsWithTabsToClose;
using tabs_closure_util::GetTabsInfoForCache;
using tabs_closure_util::GetTabsToClose;
using tabs_closure_util::WebStateIDToTime;

using TabGroupIDToIndexSet = std::map<tab_groups::TabGroupId, std::set<int>>;

// List all ContentWorlds. Necessary because calling SetWebFramesManager(...)
// with a kAllContentWorlds is not enough with FakeWebState.
constexpr web::ContentWorld kContentWorlds[] = {
    web::ContentWorld::kAllContentWorlds,
    web::ContentWorld::kPageContentWorld,
    web::ContentWorld::kIsolatedWorld,
};

// Gets the WebStateIDs from `WebStateIDToTime`.
std::set<web::WebStateID> GetWebStateIDs(WebStateIDToTime tabs) {
  std::set<web::WebStateID> expected_web_state_ids;
  for (auto const& tab : tabs) {
    expected_web_state_ids.insert(tab.first);
  }
  return expected_web_state_ids;
}

// Returns the identifier of the tab at index.
web::WebStateID GetWebStateIdentifierAt(const WebStateList* list, int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, list->count());
  return list->GetWebStateAt(index)->GetUniqueIdentifier();
}

}  // namespace

class TabsClosureUtilTest : public PlatformTest {
 public:
  TabsClosureUtilTest() {
    // Create a TestProfileIOS with required services.
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(
        SessionRestorationServiceFactory::GetInstance(),
        TestSessionRestorationService::GetTestingFactory());
    builder.AddTestingFactory(IOSChromeTabRestoreServiceFactory::GetInstance(),
                              FakeTabRestoreService::GetTestingFactory());
    builder.AddTestingFactory(TipsManagerIOSFactory::GetInstance(),
                              TipsManagerIOSFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        tab_groups::TabGroupSyncServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    scene_state_ = [[SceneState alloc] init];
    browser_ = Browser::Create(profile_.get(), scene_state_);
  }

  Browser* browser() { return browser_.get(); }

  // Information required to create a fake WebState.
  struct FakeWebStateCreationParam {
    const bool realized = false;
    const bool pinned = false;
    const bool set_last_active_time = false;
  };

  // Return value of AppendFakeWebStates.
  using AppendFakeWebStatesResult =
      std::pair<std::vector<web::WebStateID>, WebStateIDToTime>;

  // Appends fake WebStates according to params.
  AppendFakeWebStatesResult AppendFakeWebStates(
      base::Time last_navigation_time,
      std::initializer_list<FakeWebStateCreationParam> params) {
    std::vector<web::WebStateID> all_ids;
    WebStateIDToTime unrealized_tabs_id_to_time;
    all_ids.reserve(params.size());

    constexpr std::string_view kURL = "https://example.com";
    WebStateList* web_state_list = browser()->GetWebStateList();
    for (const auto& creation_param : params) {
      auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
      navigation_manager->AddItem(GURL(kURL), ui::PAGE_TRANSITION_LINK);
      web::NavigationItem* item = navigation_manager->GetItemAtIndex(0);
      item->SetTimestamp(last_navigation_time);
      navigation_manager->SetLastCommittedItem(item);

      auto web_state = std::make_unique<web::FakeWebState>();
      web_state->SetIsRealized(creation_param.realized);
      web_state->SetVisibleURL(GURL(kURL));
      web_state->SetBrowserState(browser_->GetProfile());
      web_state->SetNavigationManager(std::move(navigation_manager));
      web_state->SetNavigationItemCount(1);
      if (creation_param.set_last_active_time) {
        web_state->SetLastActiveTime(last_navigation_time);
      }

      for (const web::ContentWorld content_world : kContentWorlds) {
        web_state->SetWebFramesManager(
            content_world, std::make_unique<web::FakeWebFramesManager>());
      }

      const web::WebStateID identifier = web_state->GetUniqueIdentifier();
      const int insertion_index = web_state_list->InsertWebState(
          std::move(web_state),
          WebStateList::InsertionParams::AtIndex(web_state_list->count())
              .Pinned(creation_param.pinned));
      CHECK_GE(insertion_index, 0);
      CHECK_EQ(insertion_index, web_state_list->count() - 1);

      CHECK_EQ(GetWebStateIdentifierAt(web_state_list, insertion_index),
               identifier);

      all_ids.push_back(identifier);
      if (!creation_param.realized) {
        unrealized_tabs_id_to_time.insert(
            std::make_pair(identifier, last_navigation_time));
      }
    }

    return {std::move(all_ids), std::move(unrealized_tabs_id_to_time)};
  }

  // Appends a tab group to  `_browser` with the tabs in `indexes`. These
  // indexes in WebStateList need to have been populated beforehand.
  tab_groups::TabGroupId AppendTabGroup(const std::set<int>& indexes) {
    const auto group_id = tab_groups::TabGroupId::GenerateNew();
    browser_->GetWebStateList()->CreateGroup(
        indexes,
        tab_groups::TabGroupVisualData(u"Group",
                                       tab_groups::TabGroupColorId::kPink),
        group_id);
    return group_id;
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<ProfileIOS> profile_;
  __strong SceneState* scene_state_;
  std::unique_ptr<Browser> browser_;
};

// Tests `GetTabsInfoForCache` with several time ranges.
TEST_F(TabsClosureUtilTest, GetCountOfTabsToClose) {
  base::Time now = base::Time::Now();  // Current time for reference

  const std::pair<web::WebStateID, base::Time> tab0 = {
      web::WebStateID::NewUnique(),
      now - base::Hours(1)};  // Tab 0: Active 1 hour ago.
  const std::pair<web::WebStateID, base::Time> tab1 = {
      web::WebStateID::NewUnique(),
      now - base::Hours(3)};  // Tab 1: Active 3 hours ago.
  const std::pair<web::WebStateID, base::Time> tab2 = {
      web::WebStateID::NewUnique(),
      now - base::Minutes(15)};  // Tab 2: Active 15 minutes ago.
  const std::pair<web::WebStateID, base::Time> tab3 = {
      web::WebStateID::NewUnique(),
      now - base::Days(2)};  // Tab 3: Active 2 days ago.
  const std::pair<web::WebStateID, base::Time> tab4 = {
      web::WebStateID::NewUnique(),
      now + base::Hours(1)};  // Tab 4: Active in the future.

  const WebStateIDToTime tabs = {{tab0}, {tab1}, {tab2}, {tab3}, {tab4}};

  EXPECT_EQ(
      GetTabsInfoForCache(tabs, now - base::Hours(4), now - base::Hours(2)),
      WebStateIDToTime({{tab1}}));
  EXPECT_EQ(GetTabsInfoForCache(tabs, now - base::Hours(3), now),
            WebStateIDToTime({{tab0}, {tab1}, {tab2}}));
  EXPECT_EQ(GetTabsInfoForCache(tabs, now - base::Days(3), now),
            WebStateIDToTime({{tab0}, {tab1}, {tab2}, {tab3}}));
  EXPECT_EQ(GetTabsInfoForCache(tabs, now, now + base::Hours(2)),
            WebStateIDToTime({{tab4}}));
  EXPECT_EQ(GetTabsInfoForCache({}, now, now + base::Hours(2)),
            WebStateIDToTime({}));
}

// Tests that `CloseTabs` correctly closes the tabs within the time range, in
// this case, all tabs associated with the browser which are unrealized.
TEST_F(TabsClosureUtilTest, CloseTabs_RemoveAllTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{}, {}});

  CloseTabs(web_state_list, begin_time, end_time, unrealized_tabs_id_to_time,
            /*keep_active_tab=*/false);

  EXPECT_TRUE(web_state_list->empty());
}

// Tests that `CloseTabs` correctly closes the tabs within the time range, in
// this case, all tabs associated with the browser which are unrealized except
// the active tab.
TEST_F(TabsClosureUtilTest, CloseTabs_KeepActiveTab) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{}, {}});

  ASSERT_EQ(web_state_list->count(), 2);
  const auto web_state0_id = GetWebStateIdentifierAt(web_state_list, 0);
  web_state_list->ActivateWebStateAt(0);

  CloseTabs(web_state_list, begin_time, end_time, unrealized_tabs_id_to_time,
            /*keep_active_tab=*/true);

  ASSERT_EQ(web_state_list->count(), 1);
  EXPECT_EQ(GetWebStateIdentifierAt(web_state_list, 0), web_state0_id);
}

// Tests that `GetTabsToClose` correctly return the tabs within the time range,
// in this case, all tabs associated with the browser which are unrealized.
TEST_F(TabsClosureUtilTest, GetTabsToClose_RemoveAllTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{}, {}});

  const std::set<web::WebStateID> web_state_ids = GetTabsToClose(
      web_state_list, begin_time, end_time, unrealized_tabs_id_to_time);

  EXPECT_EQ(web_state_ids.size(), unrealized_tabs_id_to_time.size());
  EXPECT_EQ(web_state_ids, GetWebStateIDs(unrealized_tabs_id_to_time));
}

// Tests that `GetTabGroupsWithTabsToClose` correctly returns the tab groups
// with tabs within the time range, in this case, all tabs associated with the
// browser which are unrealized.
TEST_F(TabsClosureUtilTest, GetTabGroupsWithTabsToClose_RemoveAllTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{}, {}});

  const tab_groups::TabGroupId group0 = AppendTabGroup({0});
  const tab_groups::TabGroupId group1 = AppendTabGroup({1});

  const TabGroupIDToIndexSet tab_group_ids = GetTabGroupsWithTabsToClose(
      web_state_list, begin_time, end_time, unrealized_tabs_id_to_time);

  const TabGroupIDToIndexSet expected_tab_group_ids = {{group0, {0}},
                                                       {group1, {1}}};

  EXPECT_EQ(tab_group_ids.size(), 2u);
  EXPECT_EQ(tab_group_ids, expected_tab_group_ids);
}

// Tests that `GetTabGroupsWithTabsToClose` correctly returns the tab groups
// with tabs within the time range, in this case, all tabs associated with the
// browser which are unrealized.
TEST_F(TabsClosureUtilTest,
       GetTabGroupsWithTabsToClose_RemoveAllTabs_SameGroup) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{}, {}});

  const tab_groups::TabGroupId group = AppendTabGroup({0, 1});

  const TabGroupIDToIndexSet tab_group_ids = GetTabGroupsWithTabsToClose(
      web_state_list, begin_time, end_time, unrealized_tabs_id_to_time);

  const TabGroupIDToIndexSet expected_tab_group_ids = {{group, {0, 1}}};

  EXPECT_EQ(tab_group_ids.size(), 1u);
  EXPECT_EQ(tab_group_ids, expected_tab_group_ids);
}

// Tests that `GetTabGroupsWithTabsToClose` correctly returns the tab groups
// with tabs within the time range, in this case, no tabs within the time range
// are in a group.
TEST_F(TabsClosureUtilTest, GetTabGroupsWithTabsToClose_NoTabGroups) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{}, {}});

  const TabGroupIDToIndexSet tab_group_ids = GetTabGroupsWithTabsToClose(
      web_state_list, begin_time, end_time, unrealized_tabs_id_to_time);

  EXPECT_TRUE(tab_group_ids.empty());
}

// Tests that `CloseTabs` correctly closes all the tabs within the time frame,
// in this case, all tabs associated with the browser including the ones passed
// as cached information and the ones created after.
TEST_F(TabsClosureUtilTest, CloseTabs_NoMatchingTabsForDeletion) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{}, {}, {.realized = true}});

  // The unrealized webstates are passed direcly. The realized webstates will
  // be checked directly.
  CloseTabs(web_state_list, begin_time, end_time, unrealized_tabs_id_to_time,
            /*keep_active_tab=*/false);

  EXPECT_TRUE(web_state_list->empty());
}

// Tests that `GetTabsToClose` correctly return the tabs within the time frame,
// in this case, all tabs associated with the browser including the ones passed
// as cached information and the ones created after.
TEST_F(TabsClosureUtilTest, GetTabsToClose_NoMatchingTabsForDeletion) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{}, {}, {.realized = true}});

  // The unrealized webstates are passed direcly. The realized webstates will
  // be checked directly.
  const std::set<web::WebStateID> web_state_ids = GetTabsToClose(
      web_state_list, begin_time, end_time, unrealized_tabs_id_to_time);

  EXPECT_EQ(web_state_ids.size(), 3u);

  const std::set<web::WebStateID> expected_web_state_ids(all_ids.begin(),
                                                         all_ids.end());
  EXPECT_EQ(web_state_ids, expected_web_state_ids);
}

// Tests that `GetTabGroupsWithTabsToClose` correctly returns the tab groups
// with tabs within the time frame, in this case, all tabs associated with the
// browser including the ones passed as cached information and the ones created
// after.
TEST_F(TabsClosureUtilTest,
       GetTabGroupsWithTabsToClose_NoMatchingTabsForDeletion) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{}, {}, {.realized = true}});

  const tab_groups::TabGroupId group0 = AppendTabGroup({0});
  const tab_groups::TabGroupId group1 = AppendTabGroup({1});
  const tab_groups::TabGroupId group2 = AppendTabGroup({2});

  // The unrealized webstates are passed direcly. The realized webstates will
  // be checked directly.
  const TabGroupIDToIndexSet tab_group_ids = GetTabGroupsWithTabsToClose(
      web_state_list, begin_time, end_time, unrealized_tabs_id_to_time);

  const TabGroupIDToIndexSet expected_tab_group_ids = {
      {group0, {0}}, {group1, {1}}, {group2, {2}}};

  EXPECT_EQ(tab_group_ids.size(), 3u);
  EXPECT_EQ(tab_group_ids, expected_tab_group_ids);
}

// Tests that `CloseTabs` correctly closes the cached unrealized tab, but not
// the non cached unrealized tab.
TEST_F(TabsClosureUtilTest, CloseTabs_OnlyOneTabForDeletion) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{}, {}});

  ASSERT_EQ(all_ids.size(), 2u);
  const web::WebStateID web_state_id0 = all_ids[0];
  const web::WebStateID web_state_id1 = all_ids[1];
  ASSERT_NE(web_state_id0, web_state_id1);

  const auto iter = unrealized_tabs_id_to_time.find(web_state_id0);
  ASSERT_NE(iter, unrealized_tabs_id_to_time.end());
  CloseTabs(web_state_list, begin_time, end_time, {{iter->first, iter->second}},
            /*keep_active_tab=*/false);

  ASSERT_EQ(web_state_list->count(), 1);
  EXPECT_EQ(GetWebStateIdentifierAt(web_state_list, 0), web_state_id1);
}

// Tests that `GetTabsToClose` correctly return the cached unrealized tab, but
// not the non cached unrealized tab.
TEST_F(TabsClosureUtilTest, GetTabsToClose_OnlyOneTabForDeletion) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{}, {}});

  ASSERT_EQ(all_ids.size(), 2u);
  const web::WebStateID web_state_id0 = all_ids[0];
  const web::WebStateID web_state_id1 = all_ids[1];
  ASSERT_NE(web_state_id0, web_state_id1);

  const auto iter = unrealized_tabs_id_to_time.find(web_state_id0);
  ASSERT_NE(iter, unrealized_tabs_id_to_time.end());
  const std::set<web::WebStateID> web_state_ids = GetTabsToClose(
      web_state_list, begin_time, end_time, {{iter->first, iter->second}});

  ASSERT_EQ(web_state_ids.size(), 1u);
  EXPECT_TRUE(web_state_ids.contains(web_state_id0));
}

// Tests that `GetTabGroupsWithTabsToClose` correctly returns the tab groups
// with tabs within the time frame, in this case, cached unrealized tab, but not
// the non cached unrealized tab.
TEST_F(TabsClosureUtilTest, GetTabGroupsWithTabsToClose_OnlyOneTabForDeletion) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{}, {}});

  const tab_groups::TabGroupId group0 = AppendTabGroup({0});
  const tab_groups::TabGroupId _ = AppendTabGroup({1});

  ASSERT_EQ(all_ids.size(), 2u);
  const web::WebStateID web_state_id0 = all_ids[0];
  const web::WebStateID web_state_id1 = all_ids[1];
  ASSERT_NE(web_state_id0, web_state_id1);

  const auto iter = unrealized_tabs_id_to_time.find(web_state_id0);
  ASSERT_NE(iter, unrealized_tabs_id_to_time.end());

  const TabGroupIDToIndexSet tab_group_ids = GetTabGroupsWithTabsToClose(
      web_state_list, begin_time, end_time, {{iter->first, iter->second}});
  const TabGroupIDToIndexSet expected_tab_group_ids = {{group0, {0}}};

  EXPECT_EQ(tab_group_ids.size(), 1u);
  EXPECT_EQ(tab_group_ids, expected_tab_group_ids);
}

// Tests that `GetTabGroupsWithTabsToClose` correctly returns the tab groups
// with tabs within the time frame, in this case, the cached unrealized tab, but
// not the non cached unrealized tab. This test tests tabs all within the same
// group.
TEST_F(TabsClosureUtilTest,
       GetTabGroupsWithTabsToClose_OnlyOneTabForDeletion_SameGroup) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{}, {}});

  ASSERT_EQ(all_ids.size(), 2u);
  const web::WebStateID web_state_id0 = all_ids[0];
  const web::WebStateID web_state_id1 = all_ids[1];
  ASSERT_NE(web_state_id0, web_state_id1);

  const auto iter = unrealized_tabs_id_to_time.find(web_state_id0);
  ASSERT_NE(iter, unrealized_tabs_id_to_time.end());

  const tab_groups::TabGroupId group0 = AppendTabGroup({0, 1});

  const TabGroupIDToIndexSet tab_group_ids = GetTabGroupsWithTabsToClose(
      web_state_list, begin_time, end_time, {{iter->first, iter->second}});
  const TabGroupIDToIndexSet expected_tab_group_ids = {{group0, {0}}};

  EXPECT_EQ(tab_group_ids.size(), 1u);
  EXPECT_EQ(tab_group_ids, expected_tab_group_ids);
}

// Tests that `CloseTabs` doesn't close unrealized tabs when none of the cached
// tabs for deletion matches with the ones in browser.
TEST_F(TabsClosureUtilTest, CloseTabs_UnrealizedAndNotMatchingTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{}, {}});

  ASSERT_EQ(all_ids.size(), 2u);
  const web::WebStateID web_state_id0 = all_ids[0];
  const web::WebStateID web_state_id1 = all_ids[1];
  ASSERT_NE(web_state_id0, web_state_id1);

  CloseTabs(web_state_list, begin_time, end_time,
            {{web::WebStateID::NewUnique(), last_navigation_time}},
            /*keep_active_tab=*/false);

  ASSERT_EQ(web_state_list->count(), 2);
  EXPECT_EQ(GetWebStateIdentifierAt(web_state_list, 0), web_state_id0);
  EXPECT_EQ(GetWebStateIdentifierAt(web_state_list, 1), web_state_id1);
}

// Tests that `GetTabsToClose` correctly returns the unrealized tabs when none
// of the cached tabs for deletion matches with the ones in browser.
TEST_F(TabsClosureUtilTest, GetTabsToClose_UnrealizedAndNotMatchingTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{}, {}});

  const std::set<web::WebStateID> web_state_ids =
      GetTabsToClose(web_state_list, begin_time, end_time,
                     {{web::WebStateID::NewUnique(), last_navigation_time}});

  EXPECT_TRUE(web_state_ids.empty());
}

// Tests that `GetTabGroupsWithTabsToClose` correctly returns the tab groups
// with tabs within the time frame, in this case, the unrealized tabs when none
// of the cached tabs for deletion matches with the ones in browser. This test
// tests tabs all within the same group.
TEST_F(TabsClosureUtilTest,
       GetTabGroupsWithTabsToClose_UnrealizedAndNotMatchingTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{}, {}});

  const tab_groups::TabGroupId _ = AppendTabGroup({0, 1});

  const TabGroupIDToIndexSet tab_group_ids = GetTabGroupsWithTabsToClose(
      web_state_list, begin_time, end_time,
      {{web::WebStateID::NewUnique(), last_navigation_time}});

  EXPECT_TRUE(tab_group_ids.empty());
}

// Tests that `CloseTabs closes tabs within the range even if all are
// unrealized, none are cached but last active timestamp is within the selected
// range.
TEST_F(TabsClosureUtilTest, CloseTabs_UnrealizedNotCachedTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] = AppendFakeWebStates(
      last_navigation_time, {{.set_last_active_time = true}});

  CloseTabs(web_state_list, begin_time, end_time, {},
            /*keep_active_tab=*/false);

  EXPECT_TRUE(web_state_list->empty());
}

// Tests that `GetTabsToClose` returns tabs within the range even if all are
// unrealized, none are cached but last active timestamp is within the selected
// range.
TEST_F(TabsClosureUtilTest, GetTabsToClose_UnrealizedNotCachedTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] = AppendFakeWebStates(
      last_navigation_time, {{.set_last_active_time = true}});

  const std::set<web::WebStateID> web_state_ids =
      GetTabsToClose(web_state_list, begin_time, end_time, {});

  const std::set<web::WebStateID> expected_ids(all_ids.begin(), all_ids.end());

  EXPECT_EQ(web_state_ids.size(), 1u);
  EXPECT_EQ(web_state_ids, expected_ids);
}

// Tests that `CloseTabs doesn't close tabs within the range unrealized but
// cached if they're pinned.
TEST_F(TabsClosureUtilTest, CloseTabs_UnrealizedCachedPinnedTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{.pinned = true}});

  ASSERT_EQ(all_ids.size(), 1u);
  const web::WebStateID web_state_id = all_ids[0];

  CloseTabs(web_state_list, begin_time, end_time,
            {{web_state_id, last_navigation_time}},
            /*keep_active_tab=*/false);

  EXPECT_EQ(web_state_list->count(), 1);
}

// Tests that `GetTabsToClose` doesn't return the tabs within the range
// unrealized but cached if they're pinned.
TEST_F(TabsClosureUtilTest, GetTabsToClose_UnrealizedCachedPinnedTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] =
      AppendFakeWebStates(last_navigation_time, {{.pinned = true}});

  ASSERT_EQ(all_ids.size(), 1u);
  const web::WebStateID web_state_id = all_ids[0];

  const std::set<web::WebStateID> web_state_ids =
      GetTabsToClose(web_state_list, begin_time, end_time,
                     {{web_state_id, last_navigation_time}});

  EXPECT_TRUE(web_state_ids.empty());
}

// Tests that `CloseTabs doesn't close tabs within the range that are realized
// but not cached if they're pinned and realized.
TEST_F(TabsClosureUtilTest, CloseTabs_RealizedNotCachedPinnedTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] = AppendFakeWebStates(
      last_navigation_time, {{.realized = true, .pinned = true}});

  ASSERT_EQ(all_ids.size(), 1u);
  const web::WebStateID web_state_id = all_ids[0];

  CloseTabs(web_state_list, begin_time, end_time,
            {{web_state_id, last_navigation_time}},
            /*keep_active_tab=*/false);

  EXPECT_EQ(web_state_list->count(), 1);
}

// Tests that `GetTabsToClose` doesn't return the tabs within the range that are
// realized but not cached if they're pinned.
TEST_F(TabsClosureUtilTest, GetTabsToClose_RealizedNotCachedPinnedTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] = AppendFakeWebStates(
      last_navigation_time, {{.realized = true, .pinned = true}});

  ASSERT_EQ(all_ids.size(), 1u);
  const web::WebStateID web_state_id = all_ids[0];

  const std::set<web::WebStateID> web_state_ids =
      GetTabsToClose(web_state_list, begin_time, end_time,
                     {{web_state_id, last_navigation_time}});

  EXPECT_TRUE(web_state_ids.empty());
}

// Tests that `GetTabGroupsWithTabsToClose` returns tab groups with tabs within
// the range even if all are unrealized, none are cached but last active
// timestamp is within the selected range.
TEST_F(TabsClosureUtilTest,
       GetTabGroupsWithTabsToClose_UnrealizedNotCachedTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  const base::Time end_time = base::Time::Now();
  const base::Time begin_time = end_time - base::Hours(1);
  const base::Time last_navigation_time = end_time - base::Minutes(1);
  const auto [all_ids, unrealized_tabs_id_to_time] = AppendFakeWebStates(
      last_navigation_time, {{.set_last_active_time = true}});

  const tab_groups::TabGroupId group = AppendTabGroup({0});

  const TabGroupIDToIndexSet tab_group_ids =
      GetTabGroupsWithTabsToClose(web_state_list, begin_time, end_time, {});
  const TabGroupIDToIndexSet expected_tab_group_ids = {{group, {0}}};

  EXPECT_EQ(tab_group_ids.size(), 1u);
  EXPECT_EQ(tab_group_ids, expected_tab_group_ids);
}
