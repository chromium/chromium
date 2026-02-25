// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/tools/navigate_tool.h"

#import "base/memory/weak_ptr.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_observer.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/page_transition_types.h"

using ActuationResult = ActuationTool::ActuationResult;

namespace {

class TestUrlLoadingObserver : public UrlLoadingObserver {
 public:
  void TabWillLoadUrl(const GURL& url,
                      ui::PageTransition transition_type,
                      base::WeakPtr<web::WebState> web_state) override {
    last_url_ = url;
    last_transition_type_ = transition_type;
    last_web_state_ = web_state;
  }
  GURL last_url_;
  ui::PageTransition last_transition_type_ = ui::PAGE_TRANSITION_FIRST;
  base::WeakPtr<web::WebState> last_web_state_;
};

}  // namespace

class NavigateToolTest : public PlatformTest {
 public:
  NavigateToolTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingNotifierBrowserAgent::FromBrowser(browser_.get())
        ->AddObserver(&url_loading_observer_);
    UrlLoadingBrowserAgent::CreateForBrowser(browser_.get());
  }

  ~NavigateToolTest() override {
    UrlLoadingNotifierBrowserAgent::FromBrowser(browser_.get())
        ->RemoveObserver(&url_loading_observer_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  TestUrlLoadingObserver url_loading_observer_;
};

TEST_F(NavigateToolTest, Create_MissingProtoFields) {
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("https://example.com");

  base::expected<std::unique_ptr<NavigateTool>, ActuationError> result =
      NavigateTool::Create(action.navigate(), profile_.get());

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ActuationErrorCode::kCreationMissingRequiredFields,
            result.error().code);

  action.mutable_navigate()->clear_url();
  action.mutable_navigate()->set_tab_id(1);

  result = NavigateTool::Create(action.navigate(), profile_.get());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ActuationErrorCode::kCreationMissingRequiredFields,
            result.error().code);
}

TEST_F(NavigateToolTest, Create_NoWebStateForTabId) {
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("https://example.com");
  // Intentionally don't add a WebState to the browser for the target tab id.
  action.mutable_navigate()->set_tab_id(1);

  base::expected<std::unique_ptr<NavigateTool>, ActuationError> result =
      NavigateTool::Create(action.navigate(), profile_.get());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ActuationErrorCode::kCreationTargetTabNotFound,
            result.error().code);
}

TEST_F(NavigateToolTest, Execute_TabRemovedBeforeExecution) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  std::string kUrl = "https://www.example.com/";
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url(kUrl);
  action.mutable_navigate()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<NavigateTool>, ActuationError> maybe_tool =
      NavigateTool::Create(action.navigate(), profile_.get());
  EXPECT_TRUE(maybe_tool.has_value());
  std::unique_ptr<NavigateTool> tool = std::move(maybe_tool.value());

  browser_->GetWebStateList()->DetachWebStateAt(0);

  base::test::TestFuture<ActuationResult> future;
  tool->Execute(future.GetCallback());

  ActuationResult result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ActuationErrorCode::kExecutionMissingDependencies,
            result.error().code);
}

TEST_F(NavigateToolTest, Execute_InvalidUrl) {
  auto web_state = std::make_unique<web::FakeWebState>();
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("");
  action.mutable_navigate()->set_tab_id(tab_id);

  base::expected<std::unique_ptr<NavigateTool>, ActuationError> maybe_tool =
      NavigateTool::Create(action.navigate(), profile_.get());
  EXPECT_TRUE(maybe_tool.has_value());
  std::unique_ptr<NavigateTool> tool = std::move(maybe_tool.value());

  base::test::TestFuture<ActuationResult> future;
  tool->Execute(future.GetCallback());

  ActuationResult result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(ActuationErrorCode::kNavigationInvalidURL, result.error().code);
}

TEST_F(NavigateToolTest, Execute_Success) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* target_web_state =
      browser_->GetWebStateList()->GetWebStateAt(0);
  std::string kUrl = "https://www.example.com/";
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url(kUrl);
  action.mutable_navigate()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<NavigateTool>, ActuationError> maybe_tool =
      NavigateTool::Create(action.navigate(), profile_.get());
  EXPECT_TRUE(maybe_tool.has_value());
  std::unique_ptr<NavigateTool> tool = std::move(maybe_tool.value());

  base::test::TestFuture<ActuationResult> future;
  tool->Execute(future.GetCallback());

  ActuationResult result = future.Get();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(GURL(kUrl), url_loading_observer_.last_url_);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      url_loading_observer_.last_transition_type_,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL));
  EXPECT_EQ(url_loading_observer_.last_web_state_.get(), target_web_state);
}

TEST_F(NavigateToolTest,
       Execute_TargetTabInBackground_NavigatesWithoutSwitching) {
  for (int i = 0; i < 2; i++) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::AtIndex(i).Activate());
  }
  ASSERT_EQ(browser_->GetWebStateList()->GetActiveWebState(),
            browser_->GetWebStateList()->GetWebStateAt(1));

  web::WebState* target_web_state =
      browser_->GetWebStateList()->GetWebStateAt(0);
  int tab_id = target_web_state->GetUniqueIdentifier().identifier();
  std::string kUrl = "https://www.example.com/";
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url(kUrl);
  action.mutable_navigate()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<NavigateTool>, ActuationError> maybe_tool =
      NavigateTool::Create(action.navigate(), profile_.get());
  EXPECT_TRUE(maybe_tool.has_value());
  std::unique_ptr<NavigateTool> tool = std::move(maybe_tool.value());

  base::test::TestFuture<ActuationResult> future;
  tool->Execute(future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_NE(browser_->GetWebStateList()->GetActiveWebState(), target_web_state);
  EXPECT_EQ(browser_->GetWebStateList()->GetActiveWebState(),
            browser_->GetWebStateList()->GetWebStateAt(1));

  EXPECT_EQ(GURL(kUrl), url_loading_observer_.last_url_);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      url_loading_observer_.last_transition_type_,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL));
  EXPECT_EQ(url_loading_observer_.last_web_state_.get(), target_web_state);
}

TEST_F(NavigateToolTest, Execute_TabMoved_Success) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* target_web_state =
      browser_->GetWebStateList()->GetWebStateAt(0);
  std::string kUrl = "https://www.example.com/";
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url(kUrl);
  action.mutable_navigate()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<NavigateTool>, ActuationError> maybe_tool =
      NavigateTool::Create(action.navigate(), profile_.get());
  EXPECT_TRUE(maybe_tool.has_value());
  std::unique_ptr<NavigateTool> tool = std::move(maybe_tool.value());

  // Add another tab.
  auto web_state2 = std::make_unique<web::FakeWebState>();
  web_state2->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state2),
      WebStateList::InsertionParams::AtIndex(1).Activate());
  // Swap their positions.
  browser_->GetWebStateList()->MoveWebStateAt(0, 1);

  base::test::TestFuture<ActuationResult> future;
  tool->Execute(future.GetCallback());

  ActuationResult result = future.Get();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(GURL(kUrl), url_loading_observer_.last_url_);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      url_loading_observer_.last_transition_type_,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL));
  EXPECT_EQ(url_loading_observer_.last_web_state_.get(), target_web_state);
}

TEST_F(NavigateToolTest, Execute_TargetTabUnrealized) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetIsRealized(false);
  web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  std::string kUrl = "https://www.example.com/";
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url(kUrl);
  action.mutable_navigate()->set_tab_id(tab_id);

  base::expected<std::unique_ptr<NavigateTool>, ActuationError> maybe_tool =
      NavigateTool::Create(action.navigate(), profile_.get());
  EXPECT_TRUE(maybe_tool.has_value());
  std::unique_ptr<NavigateTool> tool = std::move(maybe_tool.value());

  base::test::TestFuture<ActuationResult> future;
  tool->Execute(future.GetCallback());

  ActuationResult result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ActuationErrorCode::kNavigationTabNotRealized, result.error().code);
}
