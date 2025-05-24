// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/model/new_tab_animation_tab_helper.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

using tab_groups::TabGroupId;
using tab_groups::TabGroupVisualData;

const char kURL1[] = "https://www.some.url.com";

class TabInsertionBrowserAgentTest : public PlatformTest {
 public:
  TabInsertionBrowserAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(
        profile_.get(), std::make_unique<FakeWebStateListDelegate>(
                            /*force_realization_on_activation=*/true));
    TabInsertionBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = TabInsertionBrowserAgent::FromBrowser(browser_.get());
  }

  void SetUp() override {
    PlatformTest::SetUp();
    SessionRestorationServiceFactory::GetForProfile(profile_.get())
        ->SetSessionID(browser_.get(), "browser");
  }

  void TearDown() override {
    SessionRestorationServiceFactory::GetForProfile(profile_.get())
        ->Disconnect(browser_.get());
    PlatformTest::TearDown();
  }

  const web::NavigationManager::WebLoadParams LoadParams(GURL url) {
    return LoadParams(url, ui::PAGE_TRANSITION_TYPED);
  }

  const web::NavigationManager::WebLoadParams LoadParams(
      GURL url,
      ui::PageTransition transition) {
    web::NavigationManager::WebLoadParams loadParams(url);
    loadParams.referrer = web::Referrer();
    loadParams.transition_type = transition;
    return loadParams;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<TabInsertionBrowserAgent> agent_;
};

TEST_F(TabInsertionBrowserAgentTest, InsertUrlSingle) {
  web::WebState* web_state =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), TabInsertion::Params());
  ASSERT_EQ(1, browser_->GetWebStateList()->count());
  EXPECT_EQ(web_state, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_TRUE(web_state->IsRealized());
}

// Checks that inserting a tab in the background when the WebStateList is empty
// is activating it.
TEST_F(TabInsertionBrowserAgentTest, InsertUrlSingleBackground) {
  TabInsertion::Params insertion_params;
  insertion_params.in_background = true;
  web::WebState* web_state =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  ASSERT_EQ(1, browser_->GetWebStateList()->count());
  EXPECT_EQ(web_state, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ(web_state, browser_->GetWebStateList()->GetActiveWebState());
  EXPECT_TRUE(web_state->IsRealized());
}

TEST_F(TabInsertionBrowserAgentTest, InsertUrlMultiple) {
  TabInsertion::Params insertion_params;
  insertion_params.index = 0;
  web::WebState* web_state0 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  web::WebState* web_state1 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  insertion_params.index = 1;
  web::WebState* web_state2 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);

  ASSERT_EQ(3, browser_->GetWebStateList()->count());
  EXPECT_EQ(web_state1, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ(web_state2, browser_->GetWebStateList()->GetWebStateAt(1));
  EXPECT_EQ(web_state0, browser_->GetWebStateList()->GetWebStateAt(2));
}

TEST_F(TabInsertionBrowserAgentTest, InsertUrlLazyLoad) {
  // Make sure that the web state list already has an active web state.
  web::WebState* active_web_state =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), TabInsertion::Params());
  ASSERT_EQ(active_web_state, browser_->GetWebStateList()->GetActiveWebState());

  // Insert one lazy loaded web state in background.
  TabInsertion::Params lazy_load_params_background;
  lazy_load_params_background.in_background = true;
  lazy_load_params_background.instant_load = false;
  web::WebState* unrealized_web_state_in_background = agent_->InsertWebState(
      LoadParams(GURL(kURL1)), lazy_load_params_background);

  // Insert one lazy loaded web state in foreground. Although it would be
  // unrealized on initialization, it is immediately realized on activation.
  TabInsertion::Params lazy_load_params_foreground;
  lazy_load_params_foreground.instant_load = false;
  lazy_load_params_foreground.index = 1;
  web::WebState* unrealized_web_state_in_foreground = agent_->InsertWebState(
      LoadParams(GURL(kURL1)), lazy_load_params_foreground);

  ASSERT_EQ(3, browser_->GetWebStateList()->count());
  EXPECT_EQ(unrealized_web_state_in_background,
            browser_->GetWebStateList()->GetWebStateAt(2));
  EXPECT_FALSE(unrealized_web_state_in_background->IsRealized());
  EXPECT_EQ(unrealized_web_state_in_foreground,
            browser_->GetWebStateList()->GetWebStateAt(1));
  EXPECT_TRUE(unrealized_web_state_in_foreground->IsRealized());
}

TEST_F(TabInsertionBrowserAgentTest, AppendUrlSingle) {
  web::WebState* web_state =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), TabInsertion::Params());

  ASSERT_EQ(1, browser_->GetWebStateList()->count());
  EXPECT_EQ(web_state, browser_->GetWebStateList()->GetWebStateAt(0));
}

TEST_F(TabInsertionBrowserAgentTest, AppendUrlMultiple) {
  web::WebState* web_state0 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), TabInsertion::Params());
  web::WebState* web_state1 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), TabInsertion::Params());
  web::WebState* web_state2 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), TabInsertion::Params());

  ASSERT_EQ(3, browser_->GetWebStateList()->count());
  EXPECT_EQ(web_state0, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ(web_state1, browser_->GetWebStateList()->GetWebStateAt(1));
  EXPECT_EQ(web_state2, browser_->GetWebStateList()->GetWebStateAt(2));
}

TEST_F(TabInsertionBrowserAgentTest, AddWithOrderController) {
  // Create a few tabs with the controller at the front.
  web::WebState* parent =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), TabInsertion::Params());
  agent_->InsertWebState(LoadParams(GURL(kURL1)), TabInsertion::Params());
  agent_->InsertWebState(LoadParams(GURL(kURL1)), TabInsertion::Params());

  // Add a new tab, it should be added behind the parent.
  TabInsertion::Params insertion_params_with_parent;
  insertion_params_with_parent.parent = parent;
  web::WebState* child =
      agent_->InsertWebState(LoadParams(GURL(kURL1), ui::PAGE_TRANSITION_LINK),
                             insertion_params_with_parent);
  EXPECT_EQ(browser_->GetWebStateList()->GetIndexOfWebState(parent), 0);
  EXPECT_EQ(browser_->GetWebStateList()->GetIndexOfWebState(child), 1);

  // Add another new tab without a parent, should go at the end.
  web::WebState* web_state =
      agent_->InsertWebState(LoadParams(GURL(kURL1), ui::PAGE_TRANSITION_LINK),
                             TabInsertion::Params());
  EXPECT_EQ(browser_->GetWebStateList()->GetIndexOfWebState(web_state),
            browser_->GetWebStateList()->count() - 1);

  // Same for a tab that's not opened via a LINK transition.
  web::WebState* web_state2 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), TabInsertion::Params());
  EXPECT_EQ(browser_->GetWebStateList()->GetIndexOfWebState(web_state2),
            browser_->GetWebStateList()->count() - 1);

  // Add a tab in the background. It should appear behind the opening tab.
  TabInsertion::Params insertion_params_background_1;
  insertion_params_background_1.parent = web_state;
  insertion_params_background_1.in_background = true;
  web::WebState* web_state3 =
      agent_->InsertWebState(LoadParams(GURL(kURL1), ui::PAGE_TRANSITION_LINK),
                             insertion_params_background_1);
  EXPECT_EQ(browser_->GetWebStateList()->GetIndexOfWebState(web_state3),
            browser_->GetWebStateList()->GetIndexOfWebState(web_state) + 1);

  // Add another background tab behind the one we just opened.
  TabInsertion::Params insertion_params_background_2;
  insertion_params_background_2.parent = web_state3;
  insertion_params_background_2.in_background = true;
  web::WebState* web_state4 =
      agent_->InsertWebState(LoadParams(GURL(kURL1), ui::PAGE_TRANSITION_LINK),
                             insertion_params_background_2);
  EXPECT_EQ(browser_->GetWebStateList()->GetIndexOfWebState(web_state4),
            browser_->GetWebStateList()->GetIndexOfWebState(web_state3) + 1);
}

// Tests that when web_load_params.from_external is true, a
// NewTabAnimationTabHelper is created with a boolean set to disable animation.
TEST_F(TabInsertionBrowserAgentTest, ShouldSkipNewTabAnimationTrue) {
  TabInsertion::Params skip_animation_param;
  skip_animation_param.should_skip_new_tab_animation = true;
  web::WebState* web_state =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), skip_animation_param);
  const auto* helper = NewTabAnimationTabHelper::FromWebState(web_state);
  ASSERT_NE(helper, nullptr);
  EXPECT_FALSE(helper->ShouldAnimateNewTab());
}

// Tests that when web_load_params.from_external is false, a
// NewTabAnimationTabHelper is not created.
TEST_F(TabInsertionBrowserAgentTest, ShouldSkipNewTabAnimationFalse) {
  TabInsertion::Params keep_animation_param;
  keep_animation_param.should_skip_new_tab_animation = false;
  web::WebState* web_state =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), keep_animation_param);
  const auto* helper = NewTabAnimationTabHelper::FromWebState(web_state);
  EXPECT_EQ(helper, nullptr);
}

// Tests inserting a pinned tab.
TEST_F(TabInsertionBrowserAgentTest, InsertPinned) {
  TabInsertion::Params insertion_params;
  insertion_params.index = 0;
  web::WebState* web_state0 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  web::WebState* web_state1 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  insertion_params.index = 1;
  web::WebState* web_state2 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);

  insertion_params.index = TabInsertion::kPositionAutomatically;
  insertion_params.insert_pinned = true;
  web::WebState* web_state3 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);

  ASSERT_EQ(4, browser_->GetWebStateList()->count());
  ASSERT_EQ(1, browser_->GetWebStateList()->pinned_tabs_count());
  EXPECT_EQ(web_state3, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ(web_state1, browser_->GetWebStateList()->GetWebStateAt(1));
  EXPECT_EQ(web_state2, browser_->GetWebStateList()->GetWebStateAt(2));
  EXPECT_EQ(web_state0, browser_->GetWebStateList()->GetWebStateAt(3));
}

// Tests inserting a tab in no group.
TEST_F(TabInsertionBrowserAgentTest, InsertInNoGroup) {
  TabInsertion::Params insertion_params;
  web::WebState* web_state0 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  web::WebState* web_state1 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  web::WebState* web_state2 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  WebStateList* web_state_list = browser_->GetWebStateList();
  TabGroupVisualData visual_data1 =
      TabGroupVisualData(u"Group", tab_groups::TabGroupColorId::kPink);
  web_state_list->CreateGroup({1}, visual_data1, TabGroupId::GenerateNew());
  TabGroupVisualData visual_data2 =
      TabGroupVisualData(u"Other Group", tab_groups::TabGroupColorId::kYellow);
  web_state_list->CreateGroup({2}, visual_data2, TabGroupId::GenerateNew());

  insertion_params.index = TabInsertion::kPositionAutomatically;
  web::WebState* web_state3 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);

  ASSERT_EQ(4, browser_->GetWebStateList()->count());
  EXPECT_EQ(0, browser_->GetWebStateList()->pinned_tabs_count());
  EXPECT_EQ(web_state0, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ(web_state1, browser_->GetWebStateList()->GetWebStateAt(1));
  EXPECT_EQ(web_state2, browser_->GetWebStateList()->GetWebStateAt(2));
  EXPECT_EQ(web_state3, browser_->GetWebStateList()->GetWebStateAt(3));
  const TabGroup* group = browser_->GetWebStateList()->GetGroupOfWebStateAt(3);
  EXPECT_EQ(group, nullptr);
}

// Tests inserting a tab by creating a new group.
TEST_F(TabInsertionBrowserAgentTest, InsertInNewGroup) {
  TabInsertion::Params insertion_params;
  web::WebState* web_state0 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  web::WebState* web_state1 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  web::WebState* web_state2 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  WebStateList* web_state_list = browser_->GetWebStateList();
  TabGroupVisualData visual_data1 =
      TabGroupVisualData(u"Group", tab_groups::TabGroupColorId::kPink);
  const TabGroup* group1 =
      web_state_list->CreateGroup({1}, visual_data1, TabGroupId::GenerateNew());
  TabGroupVisualData visual_data2 =
      TabGroupVisualData(u"Other Group", tab_groups::TabGroupColorId::kYellow);
  const TabGroup* group2 =
      web_state_list->CreateGroup({2}, visual_data2, TabGroupId::GenerateNew());

  insertion_params.index = TabInsertion::kPositionAutomatically;
  insertion_params.insert_in_group = true;
  web::WebState* web_state3 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);

  ASSERT_EQ(4, browser_->GetWebStateList()->count());
  EXPECT_EQ(0, browser_->GetWebStateList()->pinned_tabs_count());
  EXPECT_EQ(web_state0, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ(web_state1, browser_->GetWebStateList()->GetWebStateAt(1));
  EXPECT_EQ(web_state2, browser_->GetWebStateList()->GetWebStateAt(2));
  EXPECT_EQ(web_state3, browser_->GetWebStateList()->GetWebStateAt(3));
  const TabGroup* group = browser_->GetWebStateList()->GetGroupOfWebStateAt(3);
  EXPECT_NE(group, nullptr);
  EXPECT_NE(group, group1);
  EXPECT_NE(group, group2);
  EXPECT_EQ(group->range(), TabGroupRange(3, 1));
}

// Tests inserting a tab in an existing group on the left.
TEST_F(TabInsertionBrowserAgentTest, InsertInExistingGroupOnTheLeft) {
  TabInsertion::Params insertion_params;
  web::WebState* web_state0 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  web::WebState* web_state1 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  web::WebState* web_state2 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  WebStateList* web_state_list = browser_->GetWebStateList();
  TabGroupVisualData visual_data1 =
      TabGroupVisualData(u"Group", tab_groups::TabGroupColorId::kPink);
  const TabGroup* group1 =
      web_state_list->CreateGroup({1}, visual_data1, TabGroupId::GenerateNew());
  TabGroupVisualData visual_data2 =
      TabGroupVisualData(u"Other Group", tab_groups::TabGroupColorId::kYellow);
  const TabGroup* group2 =
      web_state_list->CreateGroup({2}, visual_data2, TabGroupId::GenerateNew());

  insertion_params.index = TabInsertion::kPositionAutomatically;
  insertion_params.insert_in_group = true;
  insertion_params.tab_group = group1->GetWeakPtr();
  web::WebState* web_state3 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);

  ASSERT_EQ(4, web_state_list->count());
  EXPECT_EQ(0, web_state_list->pinned_tabs_count());
  EXPECT_EQ(web_state0, web_state_list->GetWebStateAt(0));
  EXPECT_EQ(web_state1, web_state_list->GetWebStateAt(1));
  EXPECT_EQ(web_state3, web_state_list->GetWebStateAt(2));
  EXPECT_EQ(web_state2, web_state_list->GetWebStateAt(3));
  EXPECT_EQ(group1, web_state_list->GetGroupOfWebStateAt(1));
  EXPECT_EQ(group1, web_state_list->GetGroupOfWebStateAt(2));
  EXPECT_EQ(group1->range(), TabGroupRange(1, 2));
  EXPECT_EQ(group2, web_state_list->GetGroupOfWebStateAt(3));
  EXPECT_EQ(group2->range(), TabGroupRange(3, 1));
}

// Tests inserting a tab in an existing group on the right.
TEST_F(TabInsertionBrowserAgentTest, InsertInExistingGroupOnTheRight) {
  TabInsertion::Params insertion_params;
  web::WebState* web_state0 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  web::WebState* web_state1 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  web::WebState* web_state2 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  WebStateList* web_state_list = browser_->GetWebStateList();
  TabGroupVisualData visual_data1 =
      TabGroupVisualData(u"Group", tab_groups::TabGroupColorId::kPink);
  const TabGroup* group1 =
      web_state_list->CreateGroup({1}, visual_data1, TabGroupId::GenerateNew());
  TabGroupVisualData visual_data2 =
      TabGroupVisualData(u"Other Group", tab_groups::TabGroupColorId::kYellow);
  const TabGroup* group2 =
      web_state_list->CreateGroup({2}, visual_data2, TabGroupId::GenerateNew());

  insertion_params.index = TabInsertion::kPositionAutomatically;
  insertion_params.insert_in_group = true;
  insertion_params.tab_group = group2->GetWeakPtr();
  web::WebState* web_state3 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);

  ASSERT_EQ(4, web_state_list->count());
  EXPECT_EQ(0, web_state_list->pinned_tabs_count());
  EXPECT_EQ(web_state0, web_state_list->GetWebStateAt(0));
  EXPECT_EQ(web_state1, web_state_list->GetWebStateAt(1));
  EXPECT_EQ(web_state2, web_state_list->GetWebStateAt(2));
  EXPECT_EQ(web_state3, web_state_list->GetWebStateAt(3));
  EXPECT_EQ(group1, web_state_list->GetGroupOfWebStateAt(1));
  EXPECT_EQ(group1->range(), TabGroupRange(1, 1));
  EXPECT_EQ(group2, web_state_list->GetGroupOfWebStateAt(2));
  EXPECT_EQ(group2, web_state_list->GetGroupOfWebStateAt(3));
  EXPECT_EQ(group2->range(), TabGroupRange(2, 2));
}

// Tests inserting a tab in an existing group on the right, at the beginning of
// the group.
TEST_F(TabInsertionBrowserAgentTest,
       InsertInExistingGroupOnTheRightAtTheBeginning) {
  TabInsertion::Params insertion_params;
  web::WebState* web_state0 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  web::WebState* web_state1 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  web::WebState* web_state2 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);
  WebStateList* web_state_list = browser_->GetWebStateList();
  TabGroupVisualData visual_data1 =
      TabGroupVisualData(u"Group", tab_groups::TabGroupColorId::kPink);
  const TabGroup* group1 =
      web_state_list->CreateGroup({1}, visual_data1, TabGroupId::GenerateNew());
  TabGroupVisualData visual_data2 =
      TabGroupVisualData(u"Other Group", tab_groups::TabGroupColorId::kYellow);
  const TabGroup* group2 =
      web_state_list->CreateGroup({2}, visual_data2, TabGroupId::GenerateNew());

  insertion_params.index = 2;
  insertion_params.insert_in_group = true;
  insertion_params.tab_group = group2->GetWeakPtr();
  web::WebState* web_state3 =
      agent_->InsertWebState(LoadParams(GURL(kURL1)), insertion_params);

  ASSERT_EQ(4, web_state_list->count());
  EXPECT_EQ(0, web_state_list->pinned_tabs_count());
  EXPECT_EQ(web_state0, web_state_list->GetWebStateAt(0));
  EXPECT_EQ(web_state1, web_state_list->GetWebStateAt(1));
  EXPECT_EQ(web_state3, web_state_list->GetWebStateAt(2));
  EXPECT_EQ(web_state2, web_state_list->GetWebStateAt(3));
  EXPECT_EQ(group1, web_state_list->GetGroupOfWebStateAt(1));
  EXPECT_EQ(group1->range(), TabGroupRange(1, 1));
  EXPECT_EQ(group2, web_state_list->GetGroupOfWebStateAt(2));
  EXPECT_EQ(group2, web_state_list->GetGroupOfWebStateAt(3));
  EXPECT_EQ(group2->range(), TabGroupRange(2, 2));
}

}  // namespace
