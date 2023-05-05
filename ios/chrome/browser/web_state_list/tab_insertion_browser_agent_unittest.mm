// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/new_tab_animation_tab_helper.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kURL1[] = "https://www.some.url.com";

class TabInsertionBrowserAgentTest : public PlatformTest {
 public:
  TabInsertionBrowserAgentTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    TabInsertionBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = TabInsertionBrowserAgent::FromBrowser(browser_.get());
  }

  const web::NavigationManager::WebLoadParams Params(GURL url) {
    return Params(url, ui::PAGE_TRANSITION_TYPED);
  }

  const web::NavigationManager::WebLoadParams Params(
      GURL url,
      ui::PageTransition transition) {
    web::NavigationManager::WebLoadParams loadParams(url);
    loadParams.referrer = web::Referrer();
    loadParams.transition_type = transition;
    return loadParams;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  TabInsertionBrowserAgent* agent_;
};

}  // namespace

TEST_F(TabInsertionBrowserAgentTest, InsertUrlSingle) {
  web::WebState* web_state =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/0,
                             /*in_background=*/false,
                             /*inherit_opener=*/false,
                             /*should_show_start_surface=*/false,
                             /*should_skip_new_tab_animation=*/false);
  ASSERT_EQ(1, browser_->GetWebStateList()->count());
  EXPECT_EQ(web_state, browser_->GetWebStateList()->GetWebStateAt(0));
}

TEST_F(TabInsertionBrowserAgentTest, InsertUrlMultiple) {
  web::WebState* web_state0 =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/0,
                             /*in_background=*/false,
                             /*inherit_opener=*/false,
                             /*should_show_start_surface=*/false,
                             /*should_skip_new_tab_animation=*/false);
  web::WebState* web_state1 =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/0,
                             /*in_background=*/false,
                             /*inherit_opener=*/false,
                             /*should_show_start_surface=*/false,
                             /*should_skip_new_tab_animation=*/false);
  web::WebState* web_state2 =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/1,
                             /*in_background=*/false,
                             /*inherit_opener=*/false,
                             /*should_show_start_surface=*/false,
                             /*should_skip_new_tab_animation=*/false);

  ASSERT_EQ(3, browser_->GetWebStateList()->count());
  EXPECT_EQ(web_state1, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ(web_state2, browser_->GetWebStateList()->GetWebStateAt(1));
  EXPECT_EQ(web_state0, browser_->GetWebStateList()->GetWebStateAt(2));
}

TEST_F(TabInsertionBrowserAgentTest, AppendUrlSingle) {
  web::WebState* web_state =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/browser_->GetWebStateList()->count(),
                             /*in_background=*/false,
                             /*inherit_opener=*/false,
                             /*should_show_start_surface=*/false,
                             /*should_skip_new_tab_animation=*/false);

  ASSERT_EQ(1, browser_->GetWebStateList()->count());
  EXPECT_EQ(web_state, browser_->GetWebStateList()->GetWebStateAt(0));
}

TEST_F(TabInsertionBrowserAgentTest, AppendUrlMultiple) {
  web::WebState* web_state0 =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/browser_->GetWebStateList()->count(),
                             /*in_background=*/false,
                             /*inherit_opener=*/false,
                             /*should_show_start_surface=*/false,
                             /*should_skip_new_tab_animation=*/false);
  web::WebState* web_state1 =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/browser_->GetWebStateList()->count(),
                             /*in_background=*/false,
                             /*inherit_opener=*/false,
                             /*should_show_start_surface=*/false,
                             /*should_skip_new_tab_animation=*/false);
  web::WebState* web_state2 =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/browser_->GetWebStateList()->count(),
                             /*in_background=*/false,
                             /*inherit_opener=*/false,
                             /*should_show_start_surface=*/false,
                             /*should_skip_new_tab_animation=*/false);

  ASSERT_EQ(3, browser_->GetWebStateList()->count());
  EXPECT_EQ(web_state0, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ(web_state1, browser_->GetWebStateList()->GetWebStateAt(1));
  EXPECT_EQ(web_state2, browser_->GetWebStateList()->GetWebStateAt(2));
}

TEST_F(TabInsertionBrowserAgentTest, AddWithOrderController) {
  // Create a few tabs with the controller at the front.
  web::WebState* parent =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/browser_->GetWebStateList()->count(),
                             /*in_background=*/false,
                             /*inherit_opener=*/false,
                             /*should_show_start_surface=*/false,
                             /*should_skip_new_tab_animation=*/false);
  agent_->InsertWebState(Params(GURL(kURL1)),
                         /*parent=*/nil,
                         /*opened_by_dom=*/false,
                         /*index=*/browser_->GetWebStateList()->count(),
                         /*in_background=*/false,
                         /*inherit_opener=*/false,
                         /*should_show_start_surface=*/false,
                         /*should_skip_new_tab_animation=*/false);
  agent_->InsertWebState(Params(GURL(kURL1)),
                         /*parent=*/nil,
                         /*opened_by_dom=*/false,
                         /*index=*/browser_->GetWebStateList()->count(),
                         /*in_background=*/false,
                         /*inherit_opener=*/false,
                         /*should_show_start_surface=*/false,
                         /*should_skip_new_tab_animation=*/false);

  // Add a new tab, it should be added behind the parent.
  web::WebState* child =
      agent_->InsertWebState(Params(GURL(kURL1), ui::PAGE_TRANSITION_LINK),
                             /*parent=*/parent,
                             /*opened_by_dom=*/false,
                             /*index=*/TabInsertion::kPositionAutomatically,
                             /*in_background=*/false,
                             /*inherit_opener=*/false,
                             /*should_show_start_surface=*/false,
                             /*should_skip_new_tab_animation=*/false);
  EXPECT_EQ(browser_->GetWebStateList()->GetIndexOfWebState(parent), 0);
  EXPECT_EQ(browser_->GetWebStateList()->GetIndexOfWebState(child), 1);

  // Add another new tab without a parent, should go at the end.
  web::WebState* web_state =
      agent_->InsertWebState(Params(GURL(kURL1), ui::PAGE_TRANSITION_LINK),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/TabInsertion::kPositionAutomatically,
                             /*in_background=*/false,
                             /*inherit_opener=*/false,
                             /*should_show_start_surface=*/false,
                             /*should_skip_new_tab_animation=*/false);
  EXPECT_EQ(browser_->GetWebStateList()->GetIndexOfWebState(web_state),
            browser_->GetWebStateList()->count() - 1);

  // Same for a tab that's not opened via a LINK transition.
  web::WebState* web_state2 =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/browser_->GetWebStateList()->count(),
                             /*in_background=*/false,
                             /*inherit_opener=*/false,
                             /*should_show_start_surface=*/false,
                             /*should_skip_new_tab_animation=*/false);
  EXPECT_EQ(browser_->GetWebStateList()->GetIndexOfWebState(web_state2),
            browser_->GetWebStateList()->count() - 1);

  // Add a tab in the background. It should appear behind the opening tab.
  web::WebState* web_state3 =
      agent_->InsertWebState(Params(GURL(kURL1), ui::PAGE_TRANSITION_LINK),
                             /*parent=*/web_state,
                             /*opened_by_dom=*/false,
                             /*index=*/TabInsertion::kPositionAutomatically,
                             /*in_background=*/false,
                             /*inherit_opener=*/false,
                             /*should_show_start_surface=*/false,
                             /*should_skip_new_tab_animation=*/false);
  EXPECT_EQ(browser_->GetWebStateList()->GetIndexOfWebState(web_state3),
            browser_->GetWebStateList()->GetIndexOfWebState(web_state) + 1);

  // Add another background tab behind the one we just opened.
  web::WebState* web_state4 =
      agent_->InsertWebState(Params(GURL(kURL1), ui::PAGE_TRANSITION_LINK),
                             /*parent=*/web_state3,
                             /*opened_by_dom=*/false,
                             /*index=*/TabInsertion::kPositionAutomatically,
                             /*in_background=*/false,
                             /*inherit_opener=*/false,
                             /*should_show_start_surface=*/false,
                             /*should_skip_new_tab_animation=*/false);
  EXPECT_EQ(browser_->GetWebStateList()->GetIndexOfWebState(web_state4),
            browser_->GetWebStateList()->GetIndexOfWebState(web_state3) + 1);
}

// Tests that when params.from_external is true, a NewTabAnimationTabHelper is
// created with a boolean set to disable animation.
TEST_F(TabInsertionBrowserAgentTest, ShouldSkipNewTabAnimationTrue) {
  web::WebState* web_state =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/0,
                             /*in_background=*/false,
                             /*inherit_opener=*/false,
                             /*should_show_start_surface=*/false,
                             /*should_skip_new_tab_animation=*/true);
  const auto* helper = NewTabAnimationTabHelper::FromWebState(web_state);
  ASSERT_NE(helper, nullptr);
  EXPECT_FALSE(helper->ShouldAnimateNewTab());
}

// Tests that when params.from_external is false, a NewTabAnimationTabHelper is
// not created.
TEST_F(TabInsertionBrowserAgentTest, ShouldSkipNewTabAnimationFalse) {
  web::WebState* web_state =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/0,
                             /*in_background=*/false,
                             /*inherit_opener=*/false,
                             /*should_show_start_surface=*/false,
                             /*should_skip_new_tab_animation=*/false);
  const auto* helper = NewTabAnimationTabHelper::FromWebState(web_state);
  EXPECT_EQ(helper, nullptr);
}
