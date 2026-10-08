// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/navigate_tool.h"

#import "base/memory/weak_ptr.h"
#import "base/test/gtest_util.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/origin_gating/core/origin_gating_configuration.h"
#import "components/origin_gating/core/origin_gating_registration.h"
#import "components/origin_gating/core/origin_gating_service.h"
#import "components/origin_gating/core/types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/origin_gating/model/origin_gating_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_observer.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/page_transition_types.h"

namespace actor {

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
    scoped_feature_list_.InitAndEnableFeature(kActorOriginGating);
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingNotifierBrowserAgent::FromBrowser(browser_.get())
        ->AddObserver(&url_loading_observer_);
    UrlLoadingBrowserAgent::CreateForBrowser(browser_.get());
    default_gating_registration_ =
        origin_gating::OriginGatingServiceFactory::GetForProfile(profile_.get())
            ->CreateAndRegisterChecker(
                default_gating_delegate_.GetWeakPtr(),
                origin_gating::OriginGatingConfiguration(
                    {}, origin_gating::OriginGatingConfiguration::CacheScope::
                            kOrigin));
  }

  ~NavigateToolTest() override {
    default_gating_registration_.reset();
    UrlLoadingNotifierBrowserAgent::FromBrowser(browser_.get())
        ->RemoveObserver(&url_loading_observer_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  TestUrlLoadingObserver url_loading_observer_;
  FakeOriginGatingCheckerDelegate default_gating_delegate_{/*is_allowed=*/true};
  std::unique_ptr<origin_gating::OriginGatingRegistration>
      default_gating_registration_;

  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
  CreateToolAndValidate(const optimization_guide::proto::NavigateAction& action,
                        web::WebState* web_state,
                        std::optional<origin_gating::CheckerId>
                            gating_checker_id = std::nullopt) {
    origin_gating::CheckerId checker_id =
        gating_checker_id.value_or(default_gating_registration_->id());
    std::unique_ptr<NavigateTool> tool = NavigateTool::Create(
        web_state ? web_state->GetWeakPtr() : nullptr, action,
        UrlLoadingBrowserAgent::FromBrowser(browser_.get())->AsWeakPtr(),
        origin_gating::OriginGatingServiceFactory::GetForProfile(
            profile_.get()),
        checker_id);
    CHECK(tool);
    base::test::TestFuture<ToolExecutionResult> validate_future;
    tool->Validate(validate_future.GetCallback());
    if (!validate_future.Get().IsOk()) {
      return base::unexpected(validate_future.Get());
    }
    return tool;
  }
};

TEST_F(NavigateToolTest, Validate_MissingProtoFields) {
  optimization_guide::proto::Action action;

  action.mutable_navigate()->set_tab_id(1);

  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult> result =
      CreateToolAndValidate(action.navigate(), /*web_state=*/nullptr);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(InternalToolErrorCode::kCreationMissingRequiredFields,
            result.error().internal_code().value());
}

TEST_F(NavigateToolTest, Execute_TabRemovedBeforeExecution) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_ptr = web_state.get();
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
  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), web_state_ptr);
  EXPECT_TRUE(maybe_tool.has_value());
  std::unique_ptr<NavigateTool> tool = std::move(maybe_tool.value());

  browser_->GetWebStateList()->DetachWebStateAt(0);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(InternalToolErrorCode::kExecutionMissingDependencies,
            result.internal_code().value());
}

TEST_F(NavigateToolTest, Execute_InvalidUrl) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_ptr = web_state.get();
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("");
  action.mutable_navigate()->set_tab_id(tab_id);

  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), web_state_ptr);
  EXPECT_TRUE(maybe_tool.has_value());
  std::unique_ptr<NavigateTool> tool = std::move(maybe_tool.value());

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  ASSERT_FALSE(result.IsOk());
  EXPECT_EQ(InternalToolErrorCode::kNavigationInvalidURL,
            result.internal_code().value());
}

TEST_F(NavigateToolTest, Execute_Success) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetNavigationManager(
      std::make_unique<CompletingFakeNavigationManager>(web_state.get()));
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
  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), target_web_state);
  EXPECT_TRUE(maybe_tool.has_value());
  std::unique_ptr<NavigateTool> tool = std::move(maybe_tool.value());

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_TRUE(result.IsOk());
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
        std::make_unique<CompletingFakeNavigationManager>(web_state.get()));
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
  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), target_web_state);
  EXPECT_TRUE(maybe_tool.has_value());
  std::unique_ptr<NavigateTool> tool = std::move(maybe_tool.value());

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  EXPECT_TRUE(future.Get().IsOk());
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
      std::make_unique<CompletingFakeNavigationManager>(web_state.get()));
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
  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), target_web_state);
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

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(GURL(kUrl), url_loading_observer_.last_url_);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      url_loading_observer_.last_transition_type_,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL));
  EXPECT_EQ(url_loading_observer_.last_web_state_.get(), target_web_state);
}

TEST_F(NavigateToolTest, Execute_TargetTabUnrealized) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_ptr = web_state.get();
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

  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), web_state_ptr);
  EXPECT_TRUE(maybe_tool.has_value());
  std::unique_ptr<NavigateTool> tool = std::move(maybe_tool.value());

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(InternalToolErrorCode::kNavigationTabNotRealized,
            result.internal_code().value());
}

TEST_F(NavigateToolTest, GetToolType) {
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

  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult> result =
      CreateToolAndValidate(action.navigate(), /*web_state=*/nullptr);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value()->GetToolType(), ToolType::kNavigate);
}

// Test that navigation is blocked when origin gating policy denies it.
TEST_F(NavigateToolTest, Execute_OriginGatingBlocksNavigation) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  // Delegate configured to block.
  FakeOriginGatingCheckerDelegate delegate(/*is_allowed=*/false);
  std::unique_ptr<origin_gating::OriginGatingRegistration> registration =
      origin_gating::OriginGatingServiceFactory::GetForProfile(profile_.get())
          ->CreateAndRegisterChecker(
              delegate.GetWeakPtr(),
              origin_gating::OriginGatingConfiguration(
                  {}, origin_gating::OriginGatingConfiguration::CacheScope::
                          kOrigin));
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("https://malicious.example.com/");
  action.mutable_navigate()->set_tab_id(tab_id);

  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), web_state_ptr,
                                         registration->id());
  ASSERT_TRUE(maybe_tool.has_value());

  base::test::TestFuture<ToolExecutionResult> future;
  maybe_tool.value()->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());

  EXPECT_EQ(mojom::ActionResultCode::kTriggeredNavigationBlocked,
            result.code());

  // Verify that the URL was NOT loaded.
  EXPECT_EQ(GURL(), url_loading_observer_.last_url_);
}

// Test that navigation succeeds when origin gating policy allows it.
TEST_F(NavigateToolTest, Execute_OriginGatingAllowsNavigation) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetNavigationManager(
      std::make_unique<CompletingFakeNavigationManager>(web_state.get()));
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  // Delegate configured to allow.
  FakeOriginGatingCheckerDelegate delegate(/*is_allowed=*/true);
  std::unique_ptr<origin_gating::OriginGatingRegistration> registration =
      origin_gating::OriginGatingServiceFactory::GetForProfile(profile_.get())
          ->CreateAndRegisterChecker(
              delegate.GetWeakPtr(),
              origin_gating::OriginGatingConfiguration(
                  {}, origin_gating::OriginGatingConfiguration::CacheScope::
                          kOrigin));
  const std::string kUrl = "https://safe.example.com/";
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url(kUrl);
  action.mutable_navigate()->set_tab_id(tab_id);

  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), web_state_ptr,
                                         registration->id());
  ASSERT_TRUE(maybe_tool.has_value());

  base::test::TestFuture<ToolExecutionResult> future;
  maybe_tool.value()->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(GURL(kUrl), url_loading_observer_.last_url_);
}

// Test that when the feature flag is disabled (default), origin gating is
// bypassed.
TEST_F(NavigateToolTest, Execute_OriginGatingFeatureDisabled_BypassesCheck) {
  scoped_feature_list_.Reset();

  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetNavigationManager(
      std::make_unique<CompletingFakeNavigationManager>(web_state.get()));
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  // Delegate configured to block, but the feature flag is OFF.
  FakeOriginGatingCheckerDelegate delegate(/*is_allowed=*/false);
  std::unique_ptr<origin_gating::OriginGatingRegistration> registration =
      origin_gating::OriginGatingServiceFactory::GetForProfile(profile_.get())
          ->CreateAndRegisterChecker(
              delegate.GetWeakPtr(),
              origin_gating::OriginGatingConfiguration(
                  {}, origin_gating::OriginGatingConfiguration::CacheScope::
                          kOrigin));

  const std::string kUrl = "https://example.com/";
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url(kUrl);
  action.mutable_navigate()->set_tab_id(tab_id);

  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), web_state_ptr,
                                         registration->id());
  ASSERT_TRUE(maybe_tool.has_value());

  base::test::TestFuture<ToolExecutionResult> future;
  maybe_tool.value()->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(GURL(kUrl), url_loading_observer_.last_url_);
}

// Test that navigation CHECK crashes when the feature is enabled but no
// gating checker is provided.
TEST_F(NavigateToolTest, Execute_MissingChecker_Crashes) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  const std::string kUrl = "https://example.com/";
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url(kUrl);
  action.mutable_navigate()->set_tab_id(tab_id);

  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(
          action.navigate(), web_state_ptr,
          /*gating_checker_id=*/origin_gating::CheckerId());
  ASSERT_TRUE(maybe_tool.has_value());

  base::test::TestFuture<ToolExecutionResult> future;
  EXPECT_CHECK_DEATH(maybe_tool.value()->Execute(future.GetCallback()));
}

// Test that navigation fails with kNavigateCommittedErrorPage when the
// navigation finishes without committing.
TEST_F(NavigateToolTest, Execute_NavigationFailsToCommit) {
  auto web_state = std::make_unique<web::FakeWebState>();
  auto nav_manager =
      std::make_unique<CompletingFakeNavigationManager>(web_state.get());
  nav_manager->set_has_committed(false);
  web_state->SetNavigationManager(std::move(nav_manager));

  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* target_web_state =
      browser_->GetWebStateList()->GetWebStateAt(0);

  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("https://www.example.com/");
  action.mutable_navigate()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), target_web_state);
  ASSERT_TRUE(maybe_tool.has_value());

  base::test::TestFuture<ToolExecutionResult> future;
  maybe_tool.value()->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(mojom::ActionResultCode::kNavigateCommittedErrorPage,
            result.code());
}

// Test that destroying the WebState while navigation is in progress returns
// kTabWentAway.
TEST_F(NavigateToolTest, Execute_WebStateDestroyedDuringNavigation) {
  auto web_state = std::make_unique<web::FakeWebState>();
  auto nav_manager =
      std::make_unique<CompletingFakeNavigationManager>(web_state.get());
  nav_manager->set_auto_complete(false);
  web_state->SetNavigationManager(std::move(nav_manager));

  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* target_web_state =
      browser_->GetWebStateList()->GetWebStateAt(0);

  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("https://www.example.com/");
  action.mutable_navigate()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), target_web_state);
  ASSERT_TRUE(maybe_tool.has_value());

  base::test::TestFuture<ToolExecutionResult> future;
  maybe_tool.value()->Execute(future.GetCallback());

  // Let the async origin gating check finish so LoadUrl starts observing.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !url_loading_observer_.last_url_.is_empty(); }));

  // Close/destroy the tab while NavigateTool is waiting for
  // DidFinishNavigation.
  browser_->GetWebStateList()->DetachWebStateAt(0);

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(mojom::ActionResultCode::kTabWentAway, result.code());
}

// Test that cancelling NavigateTool while navigation is in progress prevents
// the completion callback from running when navigation later finishes.
TEST_F(NavigateToolTest, Cancel_DuringNavigation_DoesNotRunCallback) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  auto nav_manager =
      std::make_unique<CompletingFakeNavigationManager>(web_state_ptr);
  nav_manager->set_auto_complete(false);
  web_state->SetNavigationManager(std::move(nav_manager));

  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("https://www.example.com/");
  action.mutable_navigate()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), web_state_ptr);
  ASSERT_TRUE(maybe_tool.has_value());

  base::test::TestFuture<ToolExecutionResult> future;
  maybe_tool.value()->Execute(future.GetCallback());

  // Wait for async origin gating check to finish so LoadUrl starts
  // observing the WebState before cancelling.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !url_loading_observer_.last_url_.is_empty(); }));

  maybe_tool.value()->Cancel();

  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  web_state_ptr->OnNavigationStarted(&context);
  web_state_ptr->OnNavigationFinished(&context);

  EXPECT_FALSE(future.IsReady());
}

// Test that cancelling NavigateTool before async origin gating check
// completes prevents LoadUrl and the completion callback from running.
TEST_F(NavigateToolTest, Cancel_BeforeGatingDecision_DoesNotLoadUrl) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  auto nav_manager =
      std::make_unique<CompletingFakeNavigationManager>(web_state_ptr);
  nav_manager->set_auto_complete(false);
  web_state->SetNavigationManager(std::move(nav_manager));

  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("https://www.example.com/");
  action.mutable_navigate()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), web_state_ptr);
  ASSERT_TRUE(maybe_tool.has_value());

  base::test::TestFuture<ToolExecutionResult> future;
  maybe_tool.value()->Execute(future.GetCallback());
  maybe_tool.value()->Cancel();

  FlushCurrentSequence();

  EXPECT_TRUE(url_loading_observer_.last_url_.is_empty());
  EXPECT_FALSE(future.IsReady());
}

// Test that NavigateTool ignores null navigation contexts while waiting for the
// target navigation to finish.
TEST_F(NavigateToolTest, Execute_IgnoresNullNavigationContext) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kActorOriginGating);

  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  auto nav_manager =
      std::make_unique<CompletingFakeNavigationManager>(web_state_ptr);
  nav_manager->set_auto_complete(false);
  web_state->SetNavigationManager(std::move(nav_manager));

  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  const GURL kTargetUrl("https://www.example.com/");
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url(kTargetUrl.spec());
  action.mutable_navigate()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), web_state_ptr);
  ASSERT_TRUE(maybe_tool.has_value());

  base::test::TestFuture<ToolExecutionResult> future;
  maybe_tool.value()->Execute(future.GetCallback());

  web_state_ptr->OnNavigationStarted(nullptr);
  web_state_ptr->OnNavigationFinished(nullptr);
  EXPECT_FALSE(future.IsReady());

  web::FakeNavigationContext valid_context;
  valid_context.SetUrl(kTargetUrl);
  valid_context.SetHasCommitted(true);
  web_state_ptr->OnNavigationStarted(&valid_context);
  web_state_ptr->OnNavigationFinished(&valid_context);
  EXPECT_TRUE(future.Get().IsOk());
}

// Test that NavigateTool ignores same-document navigations while waiting for
// the target navigation to finish.
TEST_F(NavigateToolTest, Execute_IgnoresSameDocumentNavigation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kActorOriginGating);

  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  auto nav_manager =
      std::make_unique<CompletingFakeNavigationManager>(web_state_ptr);
  nav_manager->set_auto_complete(false);
  web_state->SetNavigationManager(std::move(nav_manager));

  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  const GURL kTargetUrl("https://www.example.com/");
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url(kTargetUrl.spec());
  action.mutable_navigate()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), web_state_ptr);
  ASSERT_TRUE(maybe_tool.has_value());

  base::test::TestFuture<ToolExecutionResult> future;
  maybe_tool.value()->Execute(future.GetCallback());

  web::FakeNavigationContext same_doc_context;
  same_doc_context.SetUrl(kTargetUrl);
  same_doc_context.SetIsSameDocument(true);
  same_doc_context.SetHasCommitted(true);
  web_state_ptr->OnNavigationStarted(&same_doc_context);
  web_state_ptr->OnNavigationFinished(&same_doc_context);
  EXPECT_FALSE(future.IsReady());

  web::FakeNavigationContext valid_context;
  valid_context.SetUrl(kTargetUrl);
  valid_context.SetHasCommitted(true);
  web_state_ptr->OnNavigationStarted(&valid_context);
  web_state_ptr->OnNavigationFinished(&valid_context);
  EXPECT_TRUE(future.Get().IsOk());
}

// Test that NavigateTool ignores renderer-initiated navigations while waiting
// for the target navigation to finish.
TEST_F(NavigateToolTest, Execute_IgnoresRendererInitiatedNavigation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kActorOriginGating);

  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  auto nav_manager =
      std::make_unique<CompletingFakeNavigationManager>(web_state_ptr);
  nav_manager->set_auto_complete(false);
  web_state->SetNavigationManager(std::move(nav_manager));

  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  const GURL kTargetUrl("https://www.example.com/");
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url(kTargetUrl.spec());
  action.mutable_navigate()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), web_state_ptr);
  ASSERT_TRUE(maybe_tool.has_value());

  base::test::TestFuture<ToolExecutionResult> future;
  maybe_tool.value()->Execute(future.GetCallback());

  web::FakeNavigationContext renderer_context;
  renderer_context.SetUrl(kTargetUrl);
  renderer_context.SetIsRendererInitiated(true);
  renderer_context.SetHasCommitted(true);
  web_state_ptr->OnNavigationStarted(&renderer_context);
  web_state_ptr->OnNavigationFinished(&renderer_context);
  EXPECT_FALSE(future.IsReady());

  web::FakeNavigationContext valid_context;
  valid_context.SetUrl(kTargetUrl);
  valid_context.SetHasCommitted(true);
  web_state_ptr->OnNavigationStarted(&valid_context);
  web_state_ptr->OnNavigationFinished(&valid_context);
  EXPECT_TRUE(future.Get().IsOk());
}

// Test that NavigateTool ignores navigations to a different URL while waiting
// for the target navigation to finish.
TEST_F(NavigateToolTest, Execute_IgnoresDifferentUrlNavigation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kActorOriginGating);

  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  auto nav_manager =
      std::make_unique<CompletingFakeNavigationManager>(web_state_ptr);
  nav_manager->set_auto_complete(false);
  web_state->SetNavigationManager(std::move(nav_manager));

  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  const GURL kTargetUrl("https://www.example.com/");
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url(kTargetUrl.spec());
  action.mutable_navigate()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), web_state_ptr);
  ASSERT_TRUE(maybe_tool.has_value());

  base::test::TestFuture<ToolExecutionResult> future;
  maybe_tool.value()->Execute(future.GetCallback());

  web::FakeNavigationContext other_url_context;
  other_url_context.SetUrl(GURL("https://www.other.com/"));
  other_url_context.SetHasCommitted(true);
  web_state_ptr->OnNavigationStarted(&other_url_context);
  web_state_ptr->OnNavigationFinished(&other_url_context);
  EXPECT_FALSE(future.IsReady());

  web::FakeNavigationContext valid_context;
  valid_context.SetUrl(kTargetUrl);
  valid_context.SetHasCommitted(true);
  web_state_ptr->OnNavigationStarted(&valid_context);
  web_state_ptr->OnNavigationFinished(&valid_context);
  EXPECT_TRUE(future.Get().IsOk());
}

// Test that NavigateTool ignores a concurrent navigation starting while the
// target navigation is already pending.
TEST_F(NavigateToolTest, Execute_IgnoresConcurrentNavigation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kActorOriginGating);

  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  auto nav_manager =
      std::make_unique<CompletingFakeNavigationManager>(web_state_ptr);
  nav_manager->set_auto_complete(false);
  web_state->SetNavigationManager(std::move(nav_manager));

  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  const GURL kTargetUrl("https://www.example.com/");
  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url(kTargetUrl.spec());
  action.mutable_navigate()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
      maybe_tool = CreateToolAndValidate(action.navigate(), web_state_ptr);
  ASSERT_TRUE(maybe_tool.has_value());

  base::test::TestFuture<ToolExecutionResult> future;
  maybe_tool.value()->Execute(future.GetCallback());

  web::FakeNavigationContext valid_context;
  valid_context.SetUrl(kTargetUrl);
  valid_context.SetHasCommitted(true);
  web_state_ptr->OnNavigationStarted(&valid_context);

  web::FakeNavigationContext concurrent_context;
  concurrent_context.SetUrl(kTargetUrl);
  concurrent_context.SetHasCommitted(true);
  web_state_ptr->OnNavigationStarted(&concurrent_context);
  web_state_ptr->OnNavigationFinished(&concurrent_context);
  EXPECT_FALSE(future.IsReady());

  web_state_ptr->OnNavigationFinished(&valid_context);
  EXPECT_TRUE(future.Get().IsOk());
}
}  // namespace actor
