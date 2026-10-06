// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_browser_agent.h"

#import <memory>
#import <vector>

#import "base/scoped_observation.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_observer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace contextual_cueing {

namespace {

// Attaches a `ContextualCueingTabHelper` to a web state once it is realized,
// mirroring `BrowserWebStateListDelegate`, which observes the web state from
// its insertion (i.e. ahead of `ContextualCueingBrowserAgent`).
class RealizationTabHelperAttacher : public web::WebStateObserver {
 public:
  explicit RealizationTabHelperAttacher(web::WebState* web_state) {
    observation_.Observe(web_state);
  }

  // web::WebStateObserver:
  void WebStateRealized(web::WebState* web_state) override {
    observation_.Reset();
    ContextualCueingTabHelper::CreateForWebState(web_state);
  }
  void WebStateDestroyed(web::WebState* web_state) override {
    observation_.Reset();
  }

 private:
  base::ScopedObservation<web::WebState, web::WebStateObserver> observation_{
      this};
};

}  // namespace

class ContextualCueingBrowserAgentTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
  }

  void TearDown() override {
    browser_.reset();
    profile_.reset();
    PlatformTest::TearDown();
  }

  // Creates the browser agent under test and returns it.
  ContextualCueingBrowserAgent* CreateAgent() {
    ContextualCueingBrowserAgent::CreateForBrowser(browser_.get());
    return ContextualCueingBrowserAgent::FromBrowser(browser_.get());
  }

  // Inserts a realized `FakeWebState` with a `ContextualCueingTabHelper` into
  // `browser_`'s `WebStateList`. The tab helper is attached before insertion to
  // mirror production, where `TabHelperAttacher` runs from `WillAddWebState()`.
  web::FakeWebState* InsertWebStateWithTabHelper(bool activate) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    ContextualCueingTabHelper::CreateForWebState(web_state.get());
    web::FakeWebState* web_state_ptr = web_state.get();
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(activate));
    return web_state_ptr;
  }

  // Inserts and activates an unrealized `FakeWebState` that gets a
  // `ContextualCueingTabHelper` attached once it is realized.
  web::FakeWebState* InsertUnrealizedActiveWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    web_state->SetIsRealized(false);
    web::FakeWebState* web_state_ptr = web_state.get();
    attachers_.push_back(
        std::make_unique<RealizationTabHelperAttacher>(web_state_ptr));
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    return web_state_ptr;
  }

  // Returns the `ContextualCueingTabHelper` of `web_state`.
  ContextualCueingTabHelper* TabHelper(web::WebState* web_state) {
    return ContextualCueingTabHelper::FromWebState(web_state);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::vector<std::unique_ptr<RealizationTabHelperAttacher>> attachers_;
};

// Tests that the active tab's tab helper is kept in sync with Gemini (Helios)
// being invoked and dismissed.
TEST_F(ContextualCueingBrowserAgentTest, TestActiveTabFollowsGeminiInvocation) {
  web::FakeWebState* web_state = InsertWebStateWithTabHelper(/*activate=*/true);
  ContextualCueingBrowserAgent* agent = CreateAgent();
  ASSERT_TRUE(agent);
  ASSERT_TRUE(TabHelper(web_state));
  EXPECT_FALSE(TabHelper(web_state)->is_gemini_invoked());

  agent->OnFloatyInvokedChanged(/*is_invoked=*/true);
  EXPECT_TRUE(TabHelper(web_state)->is_gemini_invoked());

  agent->OnFloatyInvokedChanged(/*is_invoked=*/false);
  EXPECT_FALSE(TabHelper(web_state)->is_gemini_invoked());
}

// Tests that only the active tab is updated when Gemini (Helios) is invoked,
// and that a background tab is brought up to date once it becomes active.
TEST_F(ContextualCueingBrowserAgentTest, TestBackgroundTabSyncedOnActivation) {
  web::FakeWebState* first = InsertWebStateWithTabHelper(/*activate=*/true);
  web::FakeWebState* second = InsertWebStateWithTabHelper(/*activate=*/false);
  ContextualCueingBrowserAgent* agent = CreateAgent();
  ASSERT_TRUE(agent);
  ASSERT_TRUE(TabHelper(first));
  ASSERT_TRUE(TabHelper(second));

  agent->OnFloatyInvokedChanged(/*is_invoked=*/true);
  EXPECT_TRUE(TabHelper(first)->is_gemini_invoked());
  EXPECT_FALSE(TabHelper(second)->is_gemini_invoked());

  WebStateList* web_state_list = browser_->GetWebStateList();
  web_state_list->ActivateWebStateAt(
      web_state_list->GetIndexOfWebState(second));
  EXPECT_TRUE(TabHelper(second)->is_gemini_invoked());

  // Dismissing Gemini only updates the (new) active tab; `first` is synced
  // again when it is re-activated.
  agent->OnFloatyInvokedChanged(/*is_invoked=*/false);
  EXPECT_FALSE(TabHelper(second)->is_gemini_invoked());
  EXPECT_TRUE(TabHelper(first)->is_gemini_invoked());

  web_state_list->ActivateWebStateAt(web_state_list->GetIndexOfWebState(first));
  EXPECT_FALSE(TabHelper(first)->is_gemini_invoked());
}

// Tests that a tab inserted and activated while Gemini (Helios) is invoked
// starts out suppressed.
TEST_F(ContextualCueingBrowserAgentTest,
       TestNewlyActivatedTabInheritsGeminiInvoked) {
  ContextualCueingBrowserAgent* agent = CreateAgent();
  ASSERT_TRUE(agent);
  agent->OnFloatyInvokedChanged(/*is_invoked=*/true);

  web::FakeWebState* web_state = InsertWebStateWithTabHelper(/*activate=*/true);
  ASSERT_TRUE(TabHelper(web_state));
  EXPECT_TRUE(TabHelper(web_state)->is_gemini_invoked());
}

// Tests that web states without a tab helper and unrealized web states are
// ignored without forcing realization.
TEST_F(ContextualCueingBrowserAgentTest,
       TestIgnoresWebStatesWithoutTabHelperOrUnrealized) {
  ContextualCueingBrowserAgent* agent = CreateAgent();
  ASSERT_TRUE(agent);
  agent->OnFloatyInvokedChanged(/*is_invoked=*/true);

  auto unrealized = std::make_unique<web::FakeWebState>();
  unrealized->SetBrowserState(profile_.get());
  unrealized->SetIsRealized(false);
  web::FakeWebState* unrealized_ptr = unrealized.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(unrealized),
      WebStateList::InsertionParams::Automatic().Activate());
  EXPECT_FALSE(unrealized_ptr->IsRealized());

  auto no_tab_helper = std::make_unique<web::FakeWebState>();
  no_tab_helper->SetBrowserState(profile_.get());
  browser_->GetWebStateList()->InsertWebState(
      std::move(no_tab_helper),
      WebStateList::InsertionParams::Automatic().Activate());

  // Reaching here without a crash is the assertion; the agent must not
  // dereference a missing tab helper or realize the unrealized web state.
  EXPECT_FALSE(unrealized_ptr->IsRealized());
}

// Tests that the agent survives `Browser` destruction with tabs still attached.
TEST_F(ContextualCueingBrowserAgentTest, TestBrowserDestruction) {
  InsertWebStateWithTabHelper(/*activate=*/true);
  ContextualCueingBrowserAgent* agent = CreateAgent();
  ASSERT_TRUE(agent);
  agent->OnFloatyInvokedChanged(/*is_invoked=*/true);

  // Destroys the agent, then the `WebStateList`; must not crash or use freed
  // observations.
  browser_.reset();
}

// Tests that an active tab that is unrealized when Gemini (Helios) is invoked
// receives the suppression state once it becomes realized.
TEST_F(ContextualCueingBrowserAgentTest, TestActiveTabSyncedOnRealization) {
  ContextualCueingBrowserAgent* agent = CreateAgent();
  ASSERT_TRUE(agent);

  web::FakeWebState* web_state = InsertUnrealizedActiveWebState();
  agent->OnFloatyInvokedChanged(/*is_invoked=*/true);
  ASSERT_FALSE(web_state->IsRealized());
  EXPECT_FALSE(TabHelper(web_state));

  web_state->ForceRealized();
  ASSERT_TRUE(TabHelper(web_state));
  EXPECT_TRUE(TabHelper(web_state)->is_gemini_invoked());
}

// Tests that realization of a tab that is no longer active does not push the
// suppression state to it.
TEST_F(ContextualCueingBrowserAgentTest,
       TestRealizationOfBackgroundTabIsIgnored) {
  ContextualCueingBrowserAgent* agent = CreateAgent();
  ASSERT_TRUE(agent);
  agent->OnFloatyInvokedChanged(/*is_invoked=*/true);

  web::FakeWebState* first = InsertUnrealizedActiveWebState();

  // Activating another tab stops tracking `first`.
  web::FakeWebState* second = InsertWebStateWithTabHelper(/*activate=*/true);
  ASSERT_TRUE(TabHelper(second));
  EXPECT_TRUE(TabHelper(second)->is_gemini_invoked());

  first->ForceRealized();
  ASSERT_TRUE(TabHelper(first));
  EXPECT_FALSE(TabHelper(first)->is_gemini_invoked());
}

}  // namespace contextual_cueing
