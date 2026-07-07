// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/tab_management_tool.h"

#import "base/memory/weak_ptr.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/profile_context_resolver.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

class TabManagementToolTest : public PlatformTest {
 public:
  TabManagementToolTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());
  }

  ~TabManagementToolTest() override { browser_list_ = nullptr; }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<BrowserList> browser_list_;
};

TEST_F(TabManagementToolTest, CloseTab_Success) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebStateID tab_id = web_state->GetUniqueIdentifier();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  optimization_guide::proto::Action action;
  action.mutable_close_tab()->set_tab_id(tab_id.identifier());
  base::expected<std::unique_ptr<TabManagementTool>, ToolExecutionResult>
      maybe_tool = TabManagementTool::CreateCloseTabTool(
          action.close_tab(), ProfileContextResolver(profile_.get()));
  ASSERT_TRUE(maybe_tool.has_value());
  std::unique_ptr<TabManagementTool> tool = std::move(maybe_tool.value());

  EXPECT_EQ(1, browser->GetWebStateList()->count());
  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(0, browser->GetWebStateList()->count());
}

TEST_F(TabManagementToolTest, CloseTab_TabNotFound_Failure) {
  optimization_guide::proto::Action action;
  action.mutable_close_tab()->set_tab_id(999);

  base::expected<std::unique_ptr<TabManagementTool>, ToolExecutionResult>
      maybe_tool = TabManagementTool::CreateCloseTabTool(
          action.close_tab(), ProfileContextResolver(profile_.get()));
  ASSERT_FALSE(maybe_tool.has_value());
  EXPECT_EQ(mojom::ActionResultCode::kTabWentAway, maybe_tool.error().code());
}

TEST_F(TabManagementToolTest, CloseTab_BrowserDestroyed_Failure) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebStateID tab_id = web_state->GetUniqueIdentifier();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  optimization_guide::proto::Action action;
  action.mutable_close_tab()->set_tab_id(tab_id.identifier());
  base::expected<std::unique_ptr<TabManagementTool>, ToolExecutionResult>
      maybe_tool = TabManagementTool::CreateCloseTabTool(
          action.close_tab(), ProfileContextResolver(profile_.get()));
  ASSERT_TRUE(maybe_tool.has_value());
  std::unique_ptr<TabManagementTool> tool = std::move(maybe_tool.value());

  // Destroy the browser to invalidate browser_ weak pointer
  browser.reset();
  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kWindowWentAway, result.code());
}

TEST_F(TabManagementToolTest, CloseTab_WebStateDestroyed_Failure) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebStateID tab_id = web_state->GetUniqueIdentifier();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  optimization_guide::proto::Action action;
  action.mutable_close_tab()->set_tab_id(tab_id.identifier());
  base::expected<std::unique_ptr<TabManagementTool>, ToolExecutionResult>
      maybe_tool = TabManagementTool::CreateCloseTabTool(
          action.close_tab(), ProfileContextResolver(profile_.get()));
  ASSERT_TRUE(maybe_tool.has_value());
  std::unique_ptr<TabManagementTool> tool = std::move(maybe_tool.value());

  // Detach and destroy the web state to invalidate web_state_ weak pointer
  std::unique_ptr<web::WebState> destroyed_web_state =
      browser->GetWebStateList()->DetachWebStateAt(0);
  destroyed_web_state.reset();

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kTabWentAway, result.code());
}

TEST_F(TabManagementToolTest, CloseTab_WebStateDetached_Failure) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebStateID tab_id = web_state->GetUniqueIdentifier();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  optimization_guide::proto::Action action;
  action.mutable_close_tab()->set_tab_id(tab_id.identifier());
  base::expected<std::unique_ptr<TabManagementTool>, ToolExecutionResult>
      maybe_tool = TabManagementTool::CreateCloseTabTool(
          action.close_tab(), ProfileContextResolver(profile_.get()));
  ASSERT_TRUE(maybe_tool.has_value());
  std::unique_ptr<TabManagementTool> tool = std::move(maybe_tool.value());

  // Detach the web state from the browser, but keep it alive in
  // detached_web_state
  std::unique_ptr<web::WebState> detached_web_state =
      browser->GetWebStateList()->DetachWebStateAt(0);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kTabWentAway, result.code());
}

}  // namespace actor
