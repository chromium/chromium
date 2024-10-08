// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/tabs_closure_util.h"

#import "base/memory/raw_ptr.h"
#import "base/time/time.h"
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

// List all ContentWorlds. Necessary because calling SetWebFramesManager(...)
// with a kAllContentWorlds is not enough with FakeWebState.
constexpr web::ContentWorld kContentWorlds[] = {
    web::ContentWorld::kAllContentWorlds,
    web::ContentWorld::kPageContentWorld,
    web::ContentWorld::kIsolatedWorld,
};

// Session name used by the fake SceneState.
const char kSceneSessionID[] = "Identifier";

// Gets the WebStateIDs from `WebStateIDToTime`.
std::set<web::WebStateID> GetWebStateIDs(WebStateIDToTime tabs) {
  std::set<web::WebStateID> expected_web_state_ids;
  for (auto const& tab : tabs) {
    expected_web_state_ids.insert(tab.first);
  }
  return expected_web_state_ids;
}

}  // namespace

class TabsClosureUtilTest : public PlatformTest {
 public:
  TabsClosureUtilTest() {
    // Create a TestProfileIOS with required services.
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        SessionRestorationServiceFactory::GetInstance(),
        TestSessionRestorationService::GetTestingFactory());
    builder.AddTestingFactory(IOSChromeTabRestoreServiceFactory::GetInstance(),
                              FakeTabRestoreService::GetTestingFactory());
    profile_ = std::move(builder).Build();

    // Initialize the AuthenticationService.
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());

    scene_state_ = OCMClassMock([SceneState class]);
    OCMStub([scene_state_ sceneSessionID]).andReturn(@(kSceneSessionID));
    browser_ = Browser::Create(profile_.get(), scene_state_);
  }

  Browser* browser() { return browser_.get(); }

  web::WebState* web_state0() { return web_state0_; }
  web::WebState* web_state1() { return web_state1_; }

  // Appends two unrealized WebStates in `browser_` and sets `web_state0_` and
  // `web_state1_` with them. Returns a map with their WebState Ids and their
  // last_navigation_time;
  WebStateIDToTime AppendUnrealizedWebstates(base::Time last_navigation_time) {
    web_state0_ = AppendWebState(/*realized=*/false, last_navigation_time);
    web_state1_ = AppendWebState(/*realized=*/false, last_navigation_time);

    WebStateList* web_state_list = browser()->GetWebStateList();
    CHECK_EQ(web_state_list->count(), 2);
    CHECK_EQ(web_state_list->GetWebStateAt(0), web_state0_);
    CHECK_EQ(web_state_list->GetWebStateAt(1), web_state1_);

    return {{web_state0_->GetUniqueIdentifier(), last_navigation_time},
            {web_state1_->GetUniqueIdentifier(), last_navigation_time}};
  }

  // Appends a fake WebState in `browser_`.
  web::WebState* AppendWebState(bool realized,
                                base::Time last_navigation_time,
                                bool pinned = false) {
    const GURL url = GURL("https://example.com");
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->AddItem(url, ui::PAGE_TRANSITION_LINK);
    web::NavigationItem* navigation_item =
        navigation_manager->GetItemAtIndex(0);
    navigation_item->SetTimestamp(last_navigation_time);
    navigation_manager->SetLastCommittedItem(navigation_item);

    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetIsRealized(realized);
    web_state->SetVisibleURL(url);
    web_state->SetBrowserState(browser_->GetProfile());
    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetNavigationItemCount(1);

    for (const web::ContentWorld content_world : kContentWorlds) {
      web_state->SetWebFramesManager(
          content_world, std::make_unique<web::FakeWebFramesManager>());
    }

    WebStateList* web_state_list = browser_->GetWebStateList();
    // Force the insertion at the end. Otherwise, the opener will trigger logic
    // to move an inserted WebState close to its opener.
    const WebStateList::InsertionParams params =
        WebStateList::InsertionParams::AtIndex(web_state_list->count())
            .Pinned(pinned);
    const int insertion_index =
        web_state_list->InsertWebState(std::move(web_state), params);

    return web_state_list->GetWebStateAt(insertion_index);
  }

  // Appends a tab group to  `_browser` with the tabs in `indexes`. These
  // indexes in WebStateList need to have been populated beforehand.
  const TabGroup* AppendTabGroup(const std::set<int>& indexes) {
    tab_groups::TabGroupVisualData visualData = tab_groups::TabGroupVisualData(
        u"Group", tab_groups::TabGroupColorId::kPink);
    return browser_->GetWebStateList()->CreateGroup(
        indexes, visualData, tab_groups::TabGroupId::GenerateNew());
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<ProfileIOS> profile_;
  __strong SceneState* scene_state_;
  std::unique_ptr<Browser> browser_;

  raw_ptr<web::WebState> web_state0_;
  raw_ptr<web::WebState> web_state1_;
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
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  WebStateIDToTime tabs =
      AppendUnrealizedWebstates(end_time - base::Minutes(1));

  CloseTabs(web_state_list, begin_time, end_time, tabs,
            /*keep_active_tab=*/false);

  EXPECT_TRUE(web_state_list->empty());
}

// Tests that `CloseTabs` correctly closes the tabs within the time range, in
// this case, all tabs associated with the browser which are unrealized.
TEST_F(TabsClosureUtilTest, CloseTabs_KeepActiveTab) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  WebStateIDToTime tabs =
      AppendUnrealizedWebstates(end_time - base::Minutes(1));
  web::WebState* web_state = web_state_list->GetWebStateAt(0);
  web_state_list->ActivateWebStateAt(0);

  CloseTabs(web_state_list, begin_time, end_time, tabs,
            /*keep_active_tab=*/true);

  EXPECT_EQ(web_state_list->count(), 1);
  EXPECT_EQ(web_state_list->GetWebStateAt(0), web_state);
}

// Tests that `GetTabsToClose` correctly return the tabs within the time range,
// in this case, all tabs associated with the browser which are unrealized.
TEST_F(TabsClosureUtilTest, GetTabsToClose_RemoveAllTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  WebStateIDToTime tabs =
      AppendUnrealizedWebstates(end_time - base::Minutes(1));

  std::set<web::WebStateID> web_state_ids =
      GetTabsToClose(web_state_list, begin_time, end_time, tabs);

  EXPECT_EQ(web_state_ids.size(), tabs.size());
  EXPECT_EQ(web_state_ids, GetWebStateIDs(tabs));
}

// Tests that `GetTabGroupsWithTabsToClose` correctly returns the tab groups
// with tabs within the time range, in this case, all tabs associated with the
// browser which are unrealized.
TEST_F(TabsClosureUtilTest, GetTabGroupsWithTabsToClose_RemoveAllTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  WebStateIDToTime tabs =
      AppendUnrealizedWebstates(end_time - base::Minutes(1));
  const TabGroup* group0 = AppendTabGroup({0});
  const TabGroup* group1 = AppendTabGroup({1});

  std::map<tab_groups::TabGroupId, std::set<int>> tab_group_ids =
      GetTabGroupsWithTabsToClose(web_state_list, begin_time, end_time, tabs);

  std::map<tab_groups::TabGroupId, std::set<int>> expected_tab_group_ids = {
      {group0->tab_group_id(), {0}}, {group1->tab_group_id(), {1}}};
  EXPECT_EQ(tab_group_ids.size(), 2u);
  EXPECT_EQ(tab_group_ids, expected_tab_group_ids);
}

// Tests that `GetTabGroupsWithTabsToClose` correctly returns the tab groups
// with tabs within the time range, in this case, all tabs associated with the
// browser which are unrealized.
TEST_F(TabsClosureUtilTest,
       GetTabGroupsWithTabsToClose_RemoveAllTabs_SameGroup) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  WebStateIDToTime tabs =
      AppendUnrealizedWebstates(end_time - base::Minutes(1));
  const TabGroup* group = AppendTabGroup({0, 1});

  std::map<tab_groups::TabGroupId, std::set<int>> tab_group_ids =
      GetTabGroupsWithTabsToClose(web_state_list, begin_time, end_time, tabs);

  std::map<tab_groups::TabGroupId, std::set<int>> expected_tab_group_ids = {
      {group->tab_group_id(), {0, 1}}};
  EXPECT_EQ(tab_group_ids.size(), 1u);
  EXPECT_EQ(tab_group_ids, expected_tab_group_ids);
}

// Tests that `GetTabGroupsWithTabsToClose` correctly returns the tab groups
// with tabs within the time range, in this case, no tabs within the time range
// are in a group.
TEST_F(TabsClosureUtilTest, GetTabGroupsWithTabsToClose_NoTabGroups) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  WebStateIDToTime tabs =
      AppendUnrealizedWebstates(end_time - base::Minutes(1));

  std::map<tab_groups::TabGroupId, std::set<int>> tab_group_ids =
      GetTabGroupsWithTabsToClose(web_state_list, begin_time, end_time, tabs);

  EXPECT_TRUE(tab_group_ids.empty());
}

// Tests that `CloseTabs` correctly closes all the tabs within the time frame,
// in this case, all tabs associated with the browser including the ones passed
// as cached information and the ones created after.
TEST_F(TabsClosureUtilTest, CloseTabs_NoMatchingTabsForDeletion) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  WebStateIDToTime tabs =
      AppendUnrealizedWebstates(end_time - base::Minutes(1));
  web::WebState* web_state3_ =
      AppendWebState(/*realized=*/true, end_time - base::Minutes(1));

  ASSERT_EQ(web_state_list->count(), 3);
  ASSERT_EQ(web_state_list->GetWebStateAt(0), web_state0());
  ASSERT_EQ(web_state_list->GetWebStateAt(1), web_state1());
  ASSERT_EQ(web_state_list->GetWebStateAt(2), web_state3_);

  // The unrelized webstates are passed direcly. The realized webstates will be
  // checked directly.
  CloseTabs(web_state_list, begin_time, end_time, tabs,
            /*keep_active_tab=*/false);

  EXPECT_TRUE(web_state_list->empty());
}

// Tests that `GetTabsToClose` correctly return the tabs within the time frame,
// in this case, all tabs associated with the browser including the ones passed
// as cached information and the ones created after.
TEST_F(TabsClosureUtilTest, GetTabsToClose_NoMatchingTabsForDeletion) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  WebStateIDToTime tabs =
      AppendUnrealizedWebstates(end_time - base::Minutes(1));
  web::WebState* web_state3_ =
      AppendWebState(/*realized=*/true, end_time - base::Minutes(1));

  ASSERT_EQ(web_state_list->count(), 3);
  ASSERT_EQ(web_state_list->GetWebStateAt(0), web_state0());
  ASSERT_EQ(web_state_list->GetWebStateAt(1), web_state1());
  ASSERT_EQ(web_state_list->GetWebStateAt(2), web_state3_);

  // The unrelized webstates are passed direcly. The realized webstates will be
  // checked directly.
  std::set<web::WebStateID> web_state_ids =
      GetTabsToClose(web_state_list, begin_time, end_time, tabs);

  EXPECT_EQ(web_state_ids.size(), 3u);
  std::set<web::WebStateID> expected_web_state_ids = GetWebStateIDs(tabs);
  expected_web_state_ids.insert(web_state3_->GetUniqueIdentifier());
  EXPECT_EQ(web_state_ids, expected_web_state_ids);
}

// Tests that `GetTabGroupsWithTabsToClose` correctly returns the tab groups
// with tabs within the time frame, in this case, all tabs associated with the
// browser including the ones passed as cached information and the ones created
// after.
TEST_F(TabsClosureUtilTest,
       GetTabGroupsWithTabsToClose_NoMatchingTabsForDeletion) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  WebStateIDToTime tabs =
      AppendUnrealizedWebstates(end_time - base::Minutes(1));
  web::WebState* web_state2 =
      AppendWebState(/*realized=*/true, end_time - base::Minutes(1));

  ASSERT_EQ(web_state_list->count(), 3);
  ASSERT_EQ(web_state_list->GetWebStateAt(0), web_state0());
  ASSERT_EQ(web_state_list->GetWebStateAt(1), web_state1());
  ASSERT_EQ(web_state_list->GetWebStateAt(2), web_state2);

  const TabGroup* group0 = AppendTabGroup({0});
  const TabGroup* group1 = AppendTabGroup({1});
  const TabGroup* group2 = AppendTabGroup({2});

  // The unrelized webstates are passed direcly. The realized webstates will be
  // checked directly.
  std::map<tab_groups::TabGroupId, std::set<int>> tab_group_ids =
      GetTabGroupsWithTabsToClose(web_state_list, begin_time, end_time, tabs);

  std::map<tab_groups::TabGroupId, std::set<int>> expected_tab_group_ids = {
      {group0->tab_group_id(), {0}},
      {group1->tab_group_id(), {1}},
      {group2->tab_group_id(), {2}}};
  EXPECT_EQ(tab_group_ids.size(), 3u);
  EXPECT_EQ(tab_group_ids, expected_tab_group_ids);
}

// Tests that `CloseTabs` correctly closes the cached unrealized tab, but not
// the non cached realized tab.
TEST_F(TabsClosureUtilTest, CloseTabs_OnlyOneTabForDeletion) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  WebStateIDToTime tabs =
      AppendUnrealizedWebstates(end_time - base::Minutes(1));

  CloseTabs(web_state_list, begin_time, end_time,
            {{tabs.begin()->first, tabs.begin()->second}},
            /*keep_active_tab=*/false);

  EXPECT_EQ(web_state_list->count(), 1);
  EXPECT_EQ(web_state_list->GetWebStateAt(0), web_state1());
}

// Tests that `GetTabsToClose` correctly return the cached unrealized tab, but
// not the non cached realized tab.
TEST_F(TabsClosureUtilTest, GetTabsToClose_OnlyOneTabForDeletion) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  WebStateIDToTime tabs =
      AppendUnrealizedWebstates(end_time - base::Minutes(1));

  std::set<web::WebStateID> web_state_ids =
      GetTabsToClose(web_state_list, begin_time, end_time,
                     {{tabs.begin()->first, tabs.begin()->second}});

  EXPECT_EQ(web_state_ids.size(), 1u);
  EXPECT_TRUE(web_state_ids.contains(web_state0()->GetUniqueIdentifier()));
}

// Tests that `GetTabGroupsWithTabsToClose` correctly returns the tab groups
// with tabs within the time frame, in this case, cached unrealized tab, but not
// the non cached realized tab.
TEST_F(TabsClosureUtilTest, GetTabGroupsWithTabsToClose_OnlyOneTabForDeletion) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  WebStateIDToTime tabs =
      AppendUnrealizedWebstates(end_time - base::Minutes(1));
  const TabGroup* group0 = AppendTabGroup({0});
  AppendTabGroup({1});

  std::map<tab_groups::TabGroupId, std::set<int>> tab_group_ids =
      GetTabGroupsWithTabsToClose(
          web_state_list, begin_time, end_time,
          {{tabs.begin()->first, tabs.begin()->second}});
  std::map<tab_groups::TabGroupId, std::set<int>> expected_tab_group_ids = {
      {group0->tab_group_id(), {0}}};

  EXPECT_EQ(tab_group_ids.size(), 1u);
  EXPECT_EQ(tab_group_ids, expected_tab_group_ids);
}

// Tests that `GetTabGroupsWithTabsToClose` correctly returns the tab groups
// with tabs within the time frame, in this case, the cached unrealized tab, but
// not the non cached realized tab. This test tests tabs all within the same
// group.
TEST_F(TabsClosureUtilTest,
       GetTabGroupsWithTabsToClose_OnlyOneTabForDeletion_SameGroup) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  WebStateIDToTime tabs =
      AppendUnrealizedWebstates(end_time - base::Minutes(1));
  const TabGroup* group0 = AppendTabGroup({0, 1});

  std::map<tab_groups::TabGroupId, std::set<int>> tab_group_ids =
      GetTabGroupsWithTabsToClose(
          web_state_list, begin_time, end_time,
          {{tabs.begin()->first, tabs.begin()->second}});
  std::map<tab_groups::TabGroupId, std::set<int>> expected_tab_group_ids = {
      {group0->tab_group_id(), {0}}};

  EXPECT_EQ(tab_group_ids.size(), 1u);
  EXPECT_EQ(tab_group_ids, expected_tab_group_ids);
}

// Tests that `CloseTabs` doesn't close unrealized tabs when none of the cached
// tabs for deletion matches with the ones in browser.
TEST_F(TabsClosureUtilTest, CloseTabs_UnrealizedAndNotMatchingTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  AppendUnrealizedWebstates(end_time - base::Minutes(1));

  CloseTabs(web_state_list, begin_time, end_time,
            {{web::WebStateID::NewUnique(), end_time - base::Minutes(1)}},
            /*keep_active_tab=*/false);

  EXPECT_EQ(web_state_list->count(), 2);
  EXPECT_EQ(web_state_list->GetWebStateAt(0), web_state0());
  EXPECT_EQ(web_state_list->GetWebStateAt(1), web_state1());
}

// Tests that `GetTabsToClose` correctly returns the unrealized tabs when none
// of the cached tabs for deletion matches with the ones in browser.
TEST_F(TabsClosureUtilTest, GetTabsToClose_UnrealizedAndNotMatchingTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  AppendUnrealizedWebstates(end_time - base::Minutes(1));

  std::set<web::WebStateID> web_state_ids = GetTabsToClose(
      web_state_list, begin_time, end_time,
      {{web::WebStateID::NewUnique(), end_time - base::Minutes(1)}});

  EXPECT_TRUE(web_state_ids.empty());
}

// Tests that `GetTabGroupsWithTabsToClose` correctly returns the tab groups
// with tabs within the time frame, in this case, the unrealized tabs when none
// of the cached tabs for deletion matches with the ones in browser. This test
// tests tabs all within the same group.
TEST_F(TabsClosureUtilTest,
       GetTabGroupsWithTabsToClose_UnrealizedAndNotMatchingTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  AppendUnrealizedWebstates(end_time - base::Minutes(1));
  AppendTabGroup({0, 1});

  std::map<tab_groups::TabGroupId, std::set<int>> tab_group_ids =
      GetTabGroupsWithTabsToClose(
          web_state_list, begin_time, end_time,
          {{web::WebStateID::NewUnique(), end_time - base::Minutes(1)}});

  EXPECT_TRUE(tab_group_ids.empty());
}

// Tests that `CloseTabs closes tabs within the range even if all are
// unrealized, none are cached but last active timestamp is within the selected
// range.
TEST_F(TabsClosureUtilTest, CloseTabs_UnrealizedNotCachedTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  web::FakeWebState* webstate = static_cast<web::FakeWebState*>(
      AppendWebState(/*realized=*/false, end_time - base::Minutes(1)));
  webstate->SetLastActiveTime(end_time - base::Minutes(1));

  CloseTabs(web_state_list, begin_time, end_time, {},
            /*keep_active_tab=*/false);

  EXPECT_TRUE(web_state_list->empty());
}

// Tests that `GetTabsToClose` returns tabs within the range even if all are
// unrealized, none are cached but last active timestamp is within the selected
// range.
TEST_F(TabsClosureUtilTest, GetTabsToClose_UnrealizedNotCachedTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  web::FakeWebState* webstate = static_cast<web::FakeWebState*>(
      AppendWebState(/*realized=*/false, end_time - base::Minutes(1)));
  webstate->SetLastActiveTime(end_time - base::Minutes(1));

  std::set<web::WebStateID> web_state_ids =
      GetTabsToClose(web_state_list, begin_time, end_time, {});

  EXPECT_EQ(web_state_ids.size(), 1u);
  EXPECT_TRUE(web_state_ids.contains(webstate->GetUniqueIdentifier()));
}

// Tests that `CloseTabs doesn't close tabs within the range unrelized but
// cached if they're pinned.
TEST_F(TabsClosureUtilTest, CloseTabs_UnrealizedCachedPinnedTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);
  base::Time last_navigation_time = end_time - base::Minutes(1);

  web::FakeWebState* webstate = static_cast<web::FakeWebState*>(AppendWebState(
      /*realized=*/false, last_navigation_time, /*pinned=*/true));

  CloseTabs(web_state_list, begin_time, end_time,
            {{webstate->GetUniqueIdentifier(), last_navigation_time}},
            /*keep_active_tab=*/false);

  EXPECT_EQ(web_state_list->count(), 1);
}

// Tests that `GetTabsToClose` doesn't return the tabs within the range
// unrealized but cached if they're pinned.
TEST_F(TabsClosureUtilTest, GetTabsToClose_UnrealizedCachedPinnedTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);
  base::Time last_navigation_time = end_time - base::Minutes(1);

  web::FakeWebState* webstate = static_cast<web::FakeWebState*>(AppendWebState(
      /*realized=*/false, last_navigation_time, /*pinned=*/true));

  std::set<web::WebStateID> web_state_ids =
      GetTabsToClose(web_state_list, begin_time, end_time,
                     {{webstate->GetUniqueIdentifier(), last_navigation_time}});

  EXPECT_TRUE(web_state_ids.empty());
}

// Tests that `CloseTabs doesn't close tabs within the range that are realized
// but not cached if they're pinned and realized.
TEST_F(TabsClosureUtilTest, CloseTabs_RealizedNotCachedPinnedTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);
  base::Time last_navigation_time = end_time - base::Minutes(1);

  web::FakeWebState* webstate = static_cast<web::FakeWebState*>(
      AppendWebState(/*realized=*/true, last_navigation_time, /*pinned=*/true));

  CloseTabs(web_state_list, begin_time, end_time,
            {{webstate->GetUniqueIdentifier(), last_navigation_time}},
            /*keep_active_tab=*/false);

  EXPECT_EQ(web_state_list->count(), 1);
}

// Tests that `GetTabsToClose` doesn't return the tabs within the range that are
// realized but not cached if they're pinned.
TEST_F(TabsClosureUtilTest, GetTabsToClose_RealizedNotCachedPinnedTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);
  base::Time last_navigation_time = end_time - base::Minutes(1);

  web::FakeWebState* webstate = static_cast<web::FakeWebState*>(AppendWebState(
      /*realized=*/true, last_navigation_time, /*pinned=*/true));

  std::set<web::WebStateID> web_state_ids =
      GetTabsToClose(web_state_list, begin_time, end_time,
                     {{webstate->GetUniqueIdentifier(), last_navigation_time}});

  EXPECT_TRUE(web_state_ids.empty());
}

// Tests that `GetTabGroupsWithTabsToClose` returns tab groups with tabs within
// the range even if all are unrealized, none are cached but last active
// timestamp is within the selected range.
TEST_F(TabsClosureUtilTest,
       GetTabGroupsWithTabsToClose_UnrealizedNotCachedTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  web::FakeWebState* webstate = static_cast<web::FakeWebState*>(
      AppendWebState(/*realized=*/false, end_time - base::Minutes(1)));
  webstate->SetLastActiveTime(end_time - base::Minutes(1));
  const TabGroup* group = AppendTabGroup({0});

  std::map<tab_groups::TabGroupId, std::set<int>> tab_group_ids =
      GetTabGroupsWithTabsToClose(web_state_list, begin_time, end_time, {});
  std::map<tab_groups::TabGroupId, std::set<int>> expected_tab_group_ids = {
      {group->tab_group_id(), {0}}};

  EXPECT_EQ(tab_group_ids.size(), 1u);
  EXPECT_EQ(tab_group_ids, expected_tab_group_ids);
}
