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
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
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

}  // namespace actor
