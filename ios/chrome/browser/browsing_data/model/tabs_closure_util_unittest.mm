// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/tabs_closure_util.h"

#import "base/time/time.h"
#import "ios/chrome/browser/sessions/model/fake_tab_restore_service.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/sessions/model/test_session_restoration_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
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

using tabs_closure_util::CloseTabs;
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

class TabsClosureUtilTest : public PlatformTest {
 public:
  TabsClosureUtilTest() {
    // Create a TestChromeBrowserState with required services.
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        SessionRestorationServiceFactory::GetInstance(),
        TestSessionRestorationService::GetTestingFactory());
    builder.AddTestingFactory(IOSChromeTabRestoreServiceFactory::GetInstance(),
                              FakeTabRestoreService::GetTestingFactory());
    browser_state_ = builder.Build();

    // Initialize the AuthenticationService.
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());

    scene_state_ = OCMClassMock([SceneState class]);
    OCMStub([scene_state_ sceneSessionID]).andReturn(@(kSceneSessionID));
    browser_ = Browser::Create(browser_state_.get(), scene_state_);
  }

  Browser* browser() { return browser_.get(); }

  web::WebState* web_state0() { return web_state0_; }
  web::WebState* web_state1() { return web_state1_; }

  WebStateIDToTime CreateTabs(base::Time now) {
    return {
        {web::WebStateID::NewUnique(),
         now - base::Hours(1)},  // Tab 0: Active 1 hour ago.
        {web::WebStateID::NewUnique(),
         now - base::Hours(3)},  // Tab 1: Active 3 hours ago.
        {web::WebStateID::NewUnique(),
         now - base::Minutes(15)},  // Tab 2: Active 15 minutes ago.
        {web::WebStateID::NewUnique(),
         now - base::Days(2)},  // Tab 3: Active 2 days ago.
        {web::WebStateID::NewUnique(),
         now + base::Hours(1)}  // Tab 4: Active in the future.
    };
  }

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
                                base::Time last_navigation_time) {
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
    web_state->SetBrowserState(browser_->GetBrowserState());
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
        WebStateList::InsertionParams::AtIndex(web_state_list->count());
    const int insertion_index =
        web_state_list->InsertWebState(std::move(web_state), params);

    return web_state_list->GetWebStateAt(insertion_index);
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
  __strong SceneState* scene_state_;
  std::unique_ptr<Browser> browser_;

  web::WebState* web_state0_;
  web::WebState* web_state1_;
};

// Tests `GetTabsToClose` with several time ranges.
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

  EXPECT_EQ(GetTabsToClose(tabs, now - base::Hours(4), now - base::Hours(2)),
            WebStateIDToTime({{tab1}}));
  EXPECT_EQ(GetTabsToClose(tabs, now - base::Hours(3), now),
            WebStateIDToTime({{tab0}, {tab1}, {tab2}}));
  EXPECT_EQ(GetTabsToClose(tabs, now - base::Days(3), now),
            WebStateIDToTime({{tab0}, {tab1}, {tab2}, {tab3}}));
  EXPECT_EQ(GetTabsToClose(tabs, now, now + base::Hours(2)),
            WebStateIDToTime({{tab4}}));
  EXPECT_EQ(GetTabsToClose({}, now, now + base::Hours(2)),
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

  CloseTabs(web_state_list, begin_time, end_time, tabs);

  EXPECT_TRUE(web_state_list->empty());
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
  CloseTabs(web_state_list, begin_time, end_time, tabs);

  EXPECT_TRUE(web_state_list->empty());
}

// Tests that `CloseTabs` correctly closes the cached unreliazed tab, but not
// the non cached realized tab.
TEST_F(TabsClosureUtilTest, CloseTabs_OnlyOneTabForDeletion) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  WebStateIDToTime tabs =
      AppendUnrealizedWebstates(end_time - base::Minutes(1));

  CloseTabs(web_state_list, begin_time, end_time,
            {{tabs.begin()->first, tabs.begin()->second}});

  EXPECT_EQ(web_state_list->count(), 1);
  EXPECT_EQ(web_state_list->GetWebStateAt(0), web_state1());
}

// Tests that `CloseTabs` doesn't close unreliazed tabs when none of the cached
// tabs for deletion matches with the ones in browser.
TEST_F(TabsClosureUtilTest, CloseTabs_UnrealizedAndNotMatchingTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  AppendUnrealizedWebstates(end_time - base::Minutes(1));

  CloseTabs(web_state_list, begin_time, end_time,
            {{web::WebStateID::NewUnique(), end_time - base::Minutes(1)}});

  EXPECT_EQ(web_state_list->count(), 2);
  ASSERT_EQ(web_state_list->GetWebStateAt(0), web_state0());
  ASSERT_EQ(web_state_list->GetWebStateAt(1), web_state1());
}

// Tests that `CloseTabs closes tabs within the range even if all are
// unrealized, none are cached but last active timestamp is wihtin the selected
// range.
TEST_F(TabsClosureUtilTest, CloseTabs_UnrealizedNotCachedTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  base::Time end_time = base::Time::Now();
  base::Time begin_time = end_time - base::Hours(1);

  web::FakeWebState* webstate = static_cast<web::FakeWebState*>(
      AppendWebState(/*realized=*/false, end_time - base::Minutes(1)));
  webstate->SetLastActiveTime(end_time - base::Minutes(1));

  CloseTabs(web_state_list, begin_time, end_time, {});

  EXPECT_TRUE(web_state_list->empty());
}
