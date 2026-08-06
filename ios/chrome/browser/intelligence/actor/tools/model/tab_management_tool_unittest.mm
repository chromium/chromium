// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/tab_management_tool.h"

#import "base/memory/weak_ptr.h"
#import "base/run_loop.h"
#import "base/scoped_observation.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/fake_tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
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
  web::FakeWebState* web_state_ptr = web_state.get();
  base::WeakPtr<WebStateList> web_state_list =
      browser->GetWebStateList()->AsWeakPtr();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateCloseTabTool(web_state_ptr->GetWeakPtr(),
                                            web_state_list);
  ASSERT_TRUE(tool);

  EXPECT_EQ(1, browser->GetWebStateList()->count());

  base::test::TestFuture<ToolExecutionResult> validate_future;
  tool->Validate(validate_future.GetCallback());
  EXPECT_TRUE(validate_future.Get().IsOk());

  base::test::TestFuture<ToolExecutionResult> execute_future;
  tool->Execute(execute_future.GetCallback());

  ToolExecutionResult result = execute_future.Get();
  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(0, browser->GetWebStateList()->count());
}

TEST_F(TabManagementToolTest, CloseTab_Validate_BrowserDestroyed_Fails) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  base::WeakPtr<WebStateList> web_state_list =
      browser->GetWebStateList()->AsWeakPtr();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateCloseTabTool(web_state_ptr->GetWeakPtr(),
                                            web_state_list);
  ASSERT_TRUE(tool);

  // Destroy the browser to invalidate the WebStateList.
  browser.reset();

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Validate(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kWindowWentAway, result.code());
}

TEST_F(TabManagementToolTest, CloseTab_Validate_WebStateDestroyed_Fails) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  base::WeakPtr<WebStateList> web_state_list =
      browser->GetWebStateList()->AsWeakPtr();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateCloseTabTool(web_state_ptr->GetWeakPtr(),
                                            web_state_list);
  ASSERT_TRUE(tool);

  // Detach and destroy the web state to invalidate web_state_ weak pointer
  std::unique_ptr<web::WebState> destroyed_web_state =
      browser->GetWebStateList()->DetachWebStateAt(0);
  destroyed_web_state.reset();

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Validate(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kTabWentAway, result.code());
}

TEST_F(TabManagementToolTest, CloseTab_Validate_WebStateDetached_Fails) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  base::WeakPtr<WebStateList> web_state_list =
      browser->GetWebStateList()->AsWeakPtr();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateCloseTabTool(web_state_ptr->GetWeakPtr(),
                                            web_state_list);
  ASSERT_TRUE(tool);

  // Detach the web state from the browser, but keep it alive in
  // detached_web_state
  std::unique_ptr<web::WebState> detached_web_state =
      browser->GetWebStateList()->DetachWebStateAt(0);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Validate(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kTabWentAway, result.code());
}

TEST_F(TabManagementToolTest, CloseTab_Execute_BrowserDestroyed_Fails) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  base::WeakPtr<WebStateList> web_state_list =
      browser->GetWebStateList()->AsWeakPtr();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  // Create and validate the tool.
  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateCloseTabTool(web_state_ptr->GetWeakPtr(),
                                            web_state_list);
  ASSERT_TRUE(tool);
  base::test::TestFuture<ToolExecutionResult> validate_future;
  tool->Validate(validate_future.GetCallback());
  ASSERT_TRUE(validate_future.Get().IsOk());

  // Destroy the browser to invalidate WebStateList.
  browser.reset();

  base::test::TestFuture<ToolExecutionResult> execute_future;
  tool->Execute(execute_future.GetCallback());

  ToolExecutionResult result = execute_future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kWindowWentAway, result.code());
}

TEST_F(TabManagementToolTest, CloseTab_Execute_WebStateDestroyed_Fails) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  base::WeakPtr<WebStateList> web_state_list =
      browser->GetWebStateList()->AsWeakPtr();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  // Create and validate the tool.
  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateCloseTabTool(web_state_ptr->GetWeakPtr(),
                                            web_state_list);
  ASSERT_TRUE(tool);
  base::test::TestFuture<ToolExecutionResult> validate_future;
  tool->Validate(validate_future.GetCallback());
  ASSERT_TRUE(validate_future.Get().IsOk());

  // Detach and destroy the web state to invalidate web_state_ weak pointer.
  std::unique_ptr<web::WebState> destroyed_web_state =
      browser->GetWebStateList()->DetachWebStateAt(0);
  destroyed_web_state.reset();

  base::test::TestFuture<ToolExecutionResult> execute_future;
  tool->Execute(execute_future.GetCallback());

  ToolExecutionResult result = execute_future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kTabWentAway, result.code());
}

TEST_F(TabManagementToolTest, CloseTab_Execute_WebStateDetached_Fails) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  base::WeakPtr<WebStateList> web_state_list =
      browser->GetWebStateList()->AsWeakPtr();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  // Create and validate the tool.
  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateCloseTabTool(web_state_ptr->GetWeakPtr(),
                                            web_state_list);
  ASSERT_TRUE(tool);
  base::test::TestFuture<ToolExecutionResult> validate_future;
  tool->Validate(validate_future.GetCallback());
  ASSERT_TRUE(validate_future.Get().IsOk());

  // Detach the web state from the browser, but keep it alive in
  // detached_web_state
  std::unique_ptr<web::WebState> detached_web_state =
      browser->GetWebStateList()->DetachWebStateAt(0);

  base::test::TestFuture<ToolExecutionResult> execute_future;
  tool->Execute(execute_future.GetCallback());

  ToolExecutionResult result = execute_future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kTabWentAway, result.code());
}

// A helper observer that destroys the given tool when a WebState is removed
// from the WebStateList.
class ToolDestroyingObserver : public WebStateListObserver {
 public:
  ToolDestroyingObserver(std::unique_ptr<TabManagementTool>* tool,
                         base::OnceClosure on_web_state_detached)
      : tool_(tool), on_web_state_detached_(std::move(on_web_state_detached)) {}

  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override {
    if (change.type() == WebStateListChange::Type::kDetach) {
      tool_->reset();
      if (on_web_state_detached_) {
        std::move(on_web_state_detached_).Run();
      }
    }
  }

 private:
  raw_ptr<std::unique_ptr<TabManagementTool>> tool_;
  base::OnceClosure on_web_state_detached_;
};

// Verifies that if the CloseTab tool is used to close the actuating tab, Chrome
// doesn't crash with a UAF.
TEST_F(TabManagementToolTest, CloseTab_SynchronousDestruction_Safe) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  base::WeakPtr<web::WebState> web_state_ptr =
      browser->GetWebStateList()->GetWebStateAt(0)->GetWeakPtr();
  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateCloseTabTool(
          web_state_ptr, browser->GetWebStateList()->AsWeakPtr());
  ASSERT_TRUE(tool);

  // Set up the observer to destroy the tool synchronously during
  // CloseWebStateAt.
  base::RunLoop run_loop;
  ToolDestroyingObserver observer(
      &tool, /*on_web_state_detached=*/run_loop.QuitClosure());
  base::ScopedObservation<WebStateList, WebStateListObserver> observation(
      &observer);
  observation.Observe(browser->GetWebStateList());

  tool->Execute(base::BindOnce([](ToolExecutionResult result) {
    FAIL()
        << "Callback should not be run when tool is synchronously destroyed.";
  }));
  // Run until the tab is closed, and verify that the tool was destroyed without
  // executing the callback or causing a crash.
  run_loop.Run();
  EXPECT_EQ(nullptr, tool.get());
  EXPECT_EQ(0, browser->GetWebStateList()->count());
}

TEST_F(TabManagementToolTest, CreateTab_Success) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());

  FakeToolDelegate delegate;
  int32_t window_id = 123;
  delegate.SetWebStateListForWindowId(window_id, browser->GetWebStateList());

  optimization_guide::proto::CreateTabAction action;
  action.set_window_id(window_id);
  action.set_foreground(true);

  // Create and validate the tool.
  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateTabTool(action, &delegate);
  ASSERT_TRUE(tool);
  base::test::TestFuture<ToolExecutionResult> validate_future;
  tool->Validate(validate_future.GetCallback());
  ASSERT_TRUE(validate_future.Get().IsOk());

  EXPECT_EQ(0, browser->GetWebStateList()->count());
  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(1, browser->GetWebStateList()->count());
  EXPECT_EQ(0, browser->GetWebStateList()->active_index());
  EXPECT_EQ(ToolType::kCreateTab, tool->GetToolType());
}

// Tests that creating a tab next to the prompting tab succeeds and inserts
// the new tab at the correct index (prompting index + 1).
TEST_F(TabManagementToolTest, CreateTab_NextToPromptingTab_Success) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());

  // Insert two tabs so there is a prompting tab.
  browser->GetWebStateList()->InsertWebState(
      std::make_unique<web::FakeWebState>());
  browser->GetWebStateList()->InsertWebState(
      std::make_unique<web::FakeWebState>());

  FakeToolDelegate delegate;
  int32_t window_id = 123;
  delegate.SetWebStateListForWindowId(window_id, browser->GetWebStateList());
  // Activate the web state at index 0 to act as the prompting tab.
  browser->GetWebStateList()->ActivateWebStateAt(0);

  optimization_guide::proto::CreateTabAction action;
  action.set_window_id(window_id);
  action.set_foreground(true);

  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateTabTool(action, &delegate);
  ASSERT_TRUE(tool);
  base::test::TestFuture<ToolExecutionResult> validate_future;
  tool->Validate(validate_future.GetCallback());
  ASSERT_TRUE(validate_future.Get().IsOk());

  EXPECT_EQ(2, browser->GetWebStateList()->count());
  web::WebState* ws0 = browser->GetWebStateList()->GetWebStateAt(0);
  web::WebState* ws1 = browser->GetWebStateList()->GetWebStateAt(1);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_TRUE(result.IsOk());
  // The new tab should be inserted at index 1.
  EXPECT_EQ(3, browser->GetWebStateList()->count());
  EXPECT_EQ(1, browser->GetWebStateList()->active_index());
  EXPECT_EQ(ws0, browser->GetWebStateList()->GetWebStateAt(0));
  EXPECT_NE(ws0, browser->GetWebStateList()->GetWebStateAt(1));
  EXPECT_NE(ws1, browser->GetWebStateList()->GetWebStateAt(1));
  EXPECT_EQ(ws1, browser->GetWebStateList()->GetWebStateAt(2));
}

TEST_F(TabManagementToolTest, CreateTab_Validate_WindowNotFound_Fails) {
  FakeToolDelegate delegate;
  int32_t window_id = 999;

  optimization_guide::proto::CreateTabAction action;
  action.set_window_id(window_id);
  action.set_foreground(true);

  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateTabTool(action, &delegate);
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Validate(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kWindowWentAway, result.code());
}

TEST_F(TabManagementToolTest, CreateTab_Validate_BrowserDestroyed_Fails) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());

  FakeToolDelegate delegate;
  int32_t window_id = 123;
  delegate.SetWebStateListForWindowId(window_id, browser->GetWebStateList());

  optimization_guide::proto::CreateTabAction action;
  action.set_window_id(window_id);
  action.set_foreground(true);

  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateTabTool(action, &delegate);
  ASSERT_TRUE(tool);

  // Destroy the browser to invalidate the window ID in the delegate.
  browser.reset();

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Validate(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kWindowWentAway, result.code());
}

TEST_F(TabManagementToolTest, CreateTab_Execute_BrowserDestroyed_Fails) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());

  FakeToolDelegate delegate;
  int32_t window_id = 123;
  delegate.SetWebStateListForWindowId(window_id, browser->GetWebStateList());

  optimization_guide::proto::CreateTabAction action;
  action.set_window_id(window_id);
  action.set_foreground(true);

  // Create and validate the tool.
  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateTabTool(action, &delegate);
  ASSERT_TRUE(tool);
  base::test::TestFuture<ToolExecutionResult> validate_future;
  tool->Validate(validate_future.GetCallback());
  EXPECT_TRUE(validate_future.Get().IsOk());

  // Destroy the browser to invalidate the window ID in the delegate.
  browser.reset();

  base::test::TestFuture<ToolExecutionResult> execute_future;
  tool->Execute(execute_future.GetCallback());

  ToolExecutionResult result = execute_future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kWindowWentAway, result.code());
}

TEST_F(TabManagementToolTest, CreateTab_Execute_InsertWebStateFailed_Fails) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());

  FakeToolDelegate delegate;
  int32_t window_id = 123;
  delegate.SetWebStateListForWindowId(window_id, browser->GetWebStateList());
  delegate.set_fail_insert_web_state(true);

  optimization_guide::proto::CreateTabAction action;
  action.set_window_id(window_id);
  action.set_foreground(true);

  // Create and validate the tool.
  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateTabTool(action, &delegate);
  ASSERT_TRUE(tool);
  base::test::TestFuture<ToolExecutionResult> validate_future;
  tool->Validate(validate_future.GetCallback());
  ASSERT_TRUE(validate_future.Get().IsOk());

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kNewTabCreationFailed, result.code());
}

TEST_F(TabManagementToolTest, ActivateTab_Success) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state0 = std::make_unique<web::FakeWebState>();
  auto web_state1 = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr1 = web_state1.get();
  base::WeakPtr<WebStateList> web_state_list =
      browser->GetWebStateList()->AsWeakPtr();
  browser->GetWebStateList()->InsertWebState(std::move(web_state0));
  browser->GetWebStateList()->InsertWebState(std::move(web_state1));

  // Initially active is 0.
  browser->GetWebStateList()->ActivateWebStateAt(0);
  EXPECT_EQ(0, browser->GetWebStateList()->active_index());

  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateActivateTabTool(web_state_ptr1->GetWeakPtr(),
                                               web_state_list);
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> validate_future;
  tool->Validate(validate_future.GetCallback());
  EXPECT_TRUE(validate_future.Get().IsOk());

  base::test::TestFuture<ToolExecutionResult> execute_future;
  tool->Execute(execute_future.GetCallback());

  ToolExecutionResult result = execute_future.Get();
  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(1, browser->GetWebStateList()->active_index());
  EXPECT_EQ(ToolType::kActivateTab, tool->GetToolType());
}

TEST_F(TabManagementToolTest, ActivateTab_Validate_BrowserDestroyed_Fails) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  base::WeakPtr<WebStateList> web_state_list =
      browser->GetWebStateList()->AsWeakPtr();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateActivateTabTool(web_state_ptr->GetWeakPtr(),
                                               web_state_list);
  ASSERT_TRUE(tool);

  // Destroy the browser to invalidate the WebStateList.
  browser.reset();

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Validate(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kWindowWentAway, result.code());
}

TEST_F(TabManagementToolTest, ActivateTab_Validate_WebStateDestroyed_Fails) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  base::WeakPtr<WebStateList> web_state_list =
      browser->GetWebStateList()->AsWeakPtr();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateActivateTabTool(web_state_ptr->GetWeakPtr(),
                                               web_state_list);
  ASSERT_TRUE(tool);

  // Detach and destroy the web state to invalidate web_state_ weak pointer.
  std::unique_ptr<web::WebState> destroyed_web_state =
      browser->GetWebStateList()->DetachWebStateAt(0);
  destroyed_web_state.reset();

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Validate(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kTabWentAway, result.code());
}

TEST_F(TabManagementToolTest, ActivateTab_Validate_WebStateDetached_Fails) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  base::WeakPtr<WebStateList> web_state_list =
      browser->GetWebStateList()->AsWeakPtr();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateActivateTabTool(web_state_ptr->GetWeakPtr(),
                                               web_state_list);
  ASSERT_TRUE(tool);

  // Detach the web state from the browser, but keep it alive in
  // detached_web_state.
  std::unique_ptr<web::WebState> detached_web_state =
      browser->GetWebStateList()->DetachWebStateAt(0);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Validate(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kTabWentAway, result.code());
}

TEST_F(TabManagementToolTest, ActivateTab_Execute_BrowserDestroyed_Fails) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  base::WeakPtr<WebStateList> web_state_list =
      browser->GetWebStateList()->AsWeakPtr();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  // Create and validate the tool.
  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateActivateTabTool(web_state_ptr->GetWeakPtr(),
                                               web_state_list);
  ASSERT_TRUE(tool);
  base::test::TestFuture<ToolExecutionResult> validate_future;
  tool->Validate(validate_future.GetCallback());
  ASSERT_TRUE(validate_future.Get().IsOk());

  // Destroy the browser to invalidate WebStateList.
  browser.reset();

  base::test::TestFuture<ToolExecutionResult> execute_future;
  tool->Execute(execute_future.GetCallback());

  ToolExecutionResult result = execute_future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kWindowWentAway, result.code());
}

TEST_F(TabManagementToolTest, ActivateTab_Execute_WebStateDestroyed_Fails) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  base::WeakPtr<WebStateList> web_state_list =
      browser->GetWebStateList()->AsWeakPtr();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  // Create and validate the tool.
  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateActivateTabTool(web_state_ptr->GetWeakPtr(),
                                               web_state_list);
  ASSERT_TRUE(tool);
  base::test::TestFuture<ToolExecutionResult> validate_future;
  tool->Validate(validate_future.GetCallback());
  ASSERT_TRUE(validate_future.Get().IsOk());

  // Detach and destroy the web state to invalidate web_state_ weak pointer.
  std::unique_ptr<web::WebState> destroyed_web_state =
      browser->GetWebStateList()->DetachWebStateAt(0);
  destroyed_web_state.reset();

  base::test::TestFuture<ToolExecutionResult> execute_future;
  tool->Execute(execute_future.GetCallback());

  ToolExecutionResult result = execute_future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kTabWentAway, result.code());
}

TEST_F(TabManagementToolTest, ActivateTab_Execute_WebStateDetached_Fails) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  base::WeakPtr<WebStateList> web_state_list =
      browser->GetWebStateList()->AsWeakPtr();
  browser->GetWebStateList()->InsertWebState(std::move(web_state));

  // Create and validate the tool.
  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateActivateTabTool(web_state_ptr->GetWeakPtr(),
                                               web_state_list);
  ASSERT_TRUE(tool);
  base::test::TestFuture<ToolExecutionResult> validate_future;
  tool->Validate(validate_future.GetCallback());
  ASSERT_TRUE(validate_future.Get().IsOk());

  // Detach the web state from the browser, but keep it alive in
  // detached_web_state.
  std::unique_ptr<web::WebState> detached_web_state =
      browser->GetWebStateList()->DetachWebStateAt(0);

  base::test::TestFuture<ToolExecutionResult> execute_future;
  tool->Execute(execute_future.GetCallback());

  ToolExecutionResult result = execute_future.Get();
  EXPECT_EQ(mojom::ActionResultCode::kTabWentAway, result.code());
}

// Test that creating a background tab inside a tab group succeeds and the new
// tab inherits the same group due to contiguity.
TEST_F(TabManagementToolTest, CreateTab_InTabGroup_Success) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  WebStateList* web_state_list = browser->GetWebStateList();
  FakeToolDelegate delegate;
  const int32_t window_id = 1;
  delegate.SetWebStateListForWindowId(window_id, web_state_list);

  // Insert 2 tabs and add them to a TabGroup.
  auto fake_ws0 = std::make_unique<web::FakeWebState>();
  auto fake_ws1 = std::make_unique<web::FakeWebState>();
  web::WebState* ws0 = fake_ws0.get();
  web::WebState* ws1 = fake_ws1.get();
  int index_0 = web_state_list->InsertWebState(std::move(fake_ws0));
  int index_1 = web_state_list->InsertWebState(std::move(fake_ws1));
  const TabGroup* group =
      web_state_list->CreateGroup({index_0, index_1}, /*visual_data=*/{},
                                  tab_groups::TabGroupId::GenerateNew());
  ASSERT_TRUE(group);

  // Set active index to 0 (Tab 0 is active).
  web_state_list->ActivateWebStateAt(index_0);

  // Create a tab in the background.
  optimization_guide::proto::CreateTabAction action;
  action.set_foreground(false);
  action.set_window_id(window_id);
  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateTabTool(action, &delegate);
  ASSERT_TRUE(tool);
  base::test::TestFuture<ToolExecutionResult> validate_future;
  tool->Validate(validate_future.GetCallback());
  ASSERT_TRUE(validate_future.Get().IsOk());
  base::test::TestFuture<ToolExecutionResult> execute_future;
  tool->Execute(execute_future.GetCallback());
  ASSERT_TRUE(execute_future.Get().IsOk());
  ASSERT_EQ(3, web_state_list->count());

  // The new tab should be inserted at index_1 and the tab at index_1 should be
  // moved. All tabs should still be in the tab group.
  EXPECT_EQ(web_state_list->GetWebStateAt(index_0), ws0);
  EXPECT_NE(web_state_list->GetWebStateAt(index_1), ws1);
  EXPECT_EQ(web_state_list->GetWebStateAt(index_1 + 1), ws1);
  EXPECT_EQ(group, web_state_list->GetGroupOfWebStateAt(index_0));
  EXPECT_EQ(group, web_state_list->GetGroupOfWebStateAt(index_1));
  EXPECT_EQ(group, web_state_list->GetGroupOfWebStateAt(index_1 + 1));
}

// Test that closing a tab inside a tab group succeeds and the remaining tab
// remains in the group.
TEST_F(TabManagementToolTest, CloseTab_InTabGroup_Success) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  WebStateList* web_state_list = browser->GetWebStateList();

  // Insert 2 tabs and add them to a TabGroup.
  auto fake_ws0 = std::make_unique<web::FakeWebState>();
  auto fake_ws1 = std::make_unique<web::FakeWebState>();
  int index_0 = web_state_list->InsertWebState(std::move(fake_ws0));
  int index_1 = web_state_list->InsertWebState(std::move(fake_ws1));
  web::WebState* ws0 = web_state_list->GetWebStateAt(index_0);
  const TabGroup* group =
      web_state_list->CreateGroup({index_0, index_1}, /*visual_data=*/{},
                                  tab_groups::TabGroupId::GenerateNew());
  ASSERT_TRUE(group);

  // Close the first tab.
  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateCloseTabTool(ws0->GetWeakPtr(),
                                            web_state_list->AsWeakPtr());
  ASSERT_TRUE(tool);
  base::test::TestFuture<ToolExecutionResult> validate_future;
  tool->Validate(validate_future.GetCallback());
  ASSERT_TRUE(validate_future.Get().IsOk());
  base::test::TestFuture<ToolExecutionResult> execute_future;
  tool->Execute(execute_future.GetCallback());
  ASSERT_TRUE(execute_future.Get().IsOk());

  // The remaining tab should still be in the group.
  ASSERT_EQ(1, web_state_list->count());
  EXPECT_EQ(group, web_state_list->GetGroupOfWebStateAt(0));
}

// Test that closing the only tab inside a tab group succeeds, and the empty
// tab group is correctly destroyed.
TEST_F(TabManagementToolTest, CloseTab_LastInTabGroup_Success) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  WebStateList* web_state_list = browser->GetWebStateList();

  // Create a group with the single WebState.
  auto fake_ws0 = std::make_unique<web::FakeWebState>();
  int index_0 = web_state_list->InsertWebState(std::move(fake_ws0));
  web::WebState* ws0 = web_state_list->GetWebStateAt(index_0);
  ASSERT_TRUE(ws0);
  const TabGroup* group = web_state_list->CreateGroup(
      {index_0}, /*visual_data=*/{}, tab_groups::TabGroupId::GenerateNew());
  ASSERT_TRUE(group);

  // Close the only tab in the group.
  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateCloseTabTool(ws0->GetWeakPtr(),
                                            web_state_list->AsWeakPtr());
  ASSERT_TRUE(tool);
  base::test::TestFuture<ToolExecutionResult> validate_future;
  tool->Validate(validate_future.GetCallback());
  ASSERT_TRUE(validate_future.Get().IsOk());
  base::test::TestFuture<ToolExecutionResult> execute_future;
  tool->Execute(execute_future.GetCallback());
  ASSERT_TRUE(execute_future.Get().IsOk());

  EXPECT_EQ(0, web_state_list->count());
  EXPECT_TRUE(web_state_list->GetGroups().empty());
}

// Test that activating a tab inside a tab group succeeds.
TEST_F(TabManagementToolTest, ActivateTab_InTabGroup_Success) {
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list_->AddBrowser(browser.get());
  WebStateList* web_state_list = browser->GetWebStateList();

  // Insert 2 tabs and add them to a TabGroup.
  auto fake_ws0 = std::make_unique<web::FakeWebState>();
  auto fake_ws1 = std::make_unique<web::FakeWebState>();
  int index_0 = web_state_list->InsertWebState(std::move(fake_ws0));
  int index_1 = web_state_list->InsertWebState(std::move(fake_ws1));
  web::WebState* ws1 = web_state_list->GetWebStateAt(index_1);
  ASSERT_TRUE(ws1);
  const TabGroup* group = web_state_list->CreateGroup(
      {index_0, index_1}, {}, tab_groups::TabGroupId::GenerateNew());
  ASSERT_TRUE(group);

  // Activate tab 0.
  web_state_list->ActivateWebStateAt(index_0);
  EXPECT_EQ(index_0, web_state_list->active_index());

  // Activate tab 1 via the TabManagementTool.
  std::unique_ptr<TabManagementTool> tool =
      TabManagementTool::CreateActivateTabTool(ws1->GetWeakPtr(),
                                               web_state_list->AsWeakPtr());
  ASSERT_TRUE(tool);
  base::test::TestFuture<ToolExecutionResult> validate_future;
  tool->Validate(validate_future.GetCallback());
  ASSERT_TRUE(validate_future.Get().IsOk());
  base::test::TestFuture<ToolExecutionResult> execute_future;
  tool->Execute(execute_future.GetCallback());
  ASSERT_TRUE(execute_future.Get().IsOk());

  EXPECT_EQ(index_1, web_state_list->active_index());
}

}  // namespace actor
