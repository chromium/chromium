// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/history_tool.h"

#import "base/memory/weak_ptr.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

namespace {

// Test URLs.
constexpr char kTestUrl1[] = "http://example1.com";
constexpr char kTestUrl2[] = "http://example2.com";

// Unit test to history actor tool.
class HistoryToolTest : public PlatformTest {
 public:
  HistoryToolTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());
  }

  // Inserts a web state to the test browser. This web state will have two items
  // in its navigation manager. If `first_item_active`, the active navigation
  // item will be the first one with `kTestUrl1`, otherwise it would be the
  // second navigation item with `kTestUrl2`.
  void InsertWebStateWithNavigationManager(bool first_item_active) {
    auto web_state = std::make_unique<web::FakeWebState>();
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->AddItem(GURL(kTestUrl1), ui::PAGE_TRANSITION_TYPED);
    navigation_manager->AddItem(GURL(kTestUrl2), ui::PAGE_TRANSITION_LINK);
    navigation_manager->SetLastCommittedItemIndex(first_item_active ? 0 : 1);
    web_state->SetNavigationManager(std::move(navigation_manager));
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests that the tool could not be created if proto fields are missing.
TEST_F(HistoryToolTest, Create_MissingProtoFields) {
  // Initialize the action without tab_id.
  optimization_guide::proto::Action action;
  base::expected<std::unique_ptr<HistoryTool>, ToolExecutionResult> result =
      HistoryTool::Create(action.back(), profile_.get());

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(InternalToolErrorCode::kCreationMissingRequiredFields,
            result.error().internal_code().value());

  result = HistoryTool::Create(action.forward(), profile_.get());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(InternalToolErrorCode::kCreationMissingRequiredFields,
            result.error().internal_code().value());
}

// Tests that the tool could not be created if the tab does not exist.
TEST_F(HistoryToolTest, Create_NoWebStateForTabId) {
  optimization_guide::proto::Action action;
  action.mutable_back()->set_tab_id(1);
  base::expected<std::unique_ptr<HistoryTool>, ToolExecutionResult> result =
      HistoryTool::Create(action.back(), profile_.get());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(InternalToolErrorCode::kCreationTargetTabNotFound,
            result.error().internal_code().value());
}

// Tests that the tool could not be created if the tab is removed before the
// tool is executed.
TEST_F(HistoryToolTest, Execute_TabRemovedBeforeExecution) {
  InsertWebStateWithNavigationManager(/*first_item_active=*/false);
  int tab_id = browser_->GetWebStateList()
                   ->GetWebStateAt(0)
                   ->GetUniqueIdentifier()
                   .identifier();
  optimization_guide::proto::Action action;
  action.mutable_back()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<HistoryTool>, ToolExecutionResult> maybe_tool =
      HistoryTool::Create(action.back(), profile_.get());
  EXPECT_TRUE(maybe_tool.has_value());
  std::unique_ptr<HistoryTool> tool = std::move(maybe_tool.value());

  // Removes the web state.
  browser_->GetWebStateList()->DetachWebStateAt(0);
  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(InternalToolErrorCode::kExecutionMissingDependencies,
            result.internal_code().value());
}

// Tests that the tool successfully executes and the user goes back.
TEST_F(HistoryToolTest, Execute_Back_Success) {
  InsertWebStateWithNavigationManager(/*first_item_active=*/false);
  web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(0);
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  optimization_guide::proto::Action action;
  action.mutable_back()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<HistoryTool>, ToolExecutionResult> maybe_tool =
      HistoryTool::Create(action.back(), profile_.get());
  EXPECT_TRUE(maybe_tool.has_value());

  std::unique_ptr<HistoryTool> tool = std::move(maybe_tool.value());
  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(web_state->GetNavigationManager()->GetLastCommittedItemIndex(), 0);
}

// Tests that the tool could not execute when the user should not be able to
// navigate back.
TEST_F(HistoryToolTest, Execute_Back_NotPossible) {
  InsertWebStateWithNavigationManager(/*first_item_active=*/true);
  int tab_id = browser_->GetWebStateList()
                   ->GetWebStateAt(0)
                   ->GetUniqueIdentifier()
                   .identifier();
  optimization_guide::proto::Action action;
  action.mutable_back()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<HistoryTool>, ToolExecutionResult> maybe_tool =
      HistoryTool::Create(action.back(), profile_.get());
  EXPECT_TRUE(maybe_tool.has_value());

  std::unique_ptr<HistoryTool> tool = std::move(maybe_tool.value());
  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(InternalToolErrorCode::kHistoryBackNotPossible,
            result.internal_code().value());
}

// Tests that the tool successfully executes and the user goes forward.
TEST_F(HistoryToolTest, Execute_Forward_Success) {
  InsertWebStateWithNavigationManager(/*first_item_active=*/true);
  web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(0);
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  optimization_guide::proto::Action action;
  action.mutable_forward()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<HistoryTool>, ToolExecutionResult> maybe_tool =
      HistoryTool::Create(action.forward(), profile_.get());
  EXPECT_TRUE(maybe_tool.has_value());

  std::unique_ptr<HistoryTool> tool = std::move(maybe_tool.value());
  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(web_state->GetNavigationManager()->GetLastCommittedItemIndex(), 1);
}

// Tests that the tool could not execute when the user should not be able to
// navigate forward.
TEST_F(HistoryToolTest, Execute_Forward_NotPossible) {
  InsertWebStateWithNavigationManager(/*first_item_active=*/false);
  web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(0);
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  optimization_guide::proto::Action action;
  action.mutable_forward()->set_tab_id(tab_id);
  base::expected<std::unique_ptr<HistoryTool>, ToolExecutionResult> maybe_tool =
      HistoryTool::Create(action.forward(), profile_.get());
  EXPECT_TRUE(maybe_tool.has_value());

  std::unique_ptr<HistoryTool> tool = std::move(maybe_tool.value());
  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(InternalToolErrorCode::kHistoryForwardNotPossible,
            result.internal_code().value());
}

TEST_F(HistoryToolTest, GetActionCase) {
  InsertWebStateWithNavigationManager(/*first_item_active=*/false);
  int tab_id = browser_->GetWebStateList()
                   ->GetWebStateAt(0)
                   ->GetUniqueIdentifier()
                   .identifier();

  {
    optimization_guide::proto::Action action;
    action.mutable_back()->set_tab_id(tab_id);
    auto maybe_tool = HistoryTool::Create(action.back(), profile_.get());
    ASSERT_TRUE(maybe_tool.has_value());
    EXPECT_EQ(maybe_tool.value()->GetActionCase(),
              optimization_guide::proto::Action::kBack);
  }

  {
    optimization_guide::proto::Action action;
    action.mutable_forward()->set_tab_id(tab_id);
    auto maybe_tool = HistoryTool::Create(action.forward(), profile_.get());
    ASSERT_TRUE(maybe_tool.has_value());
    EXPECT_EQ(maybe_tool.value()->GetActionCase(),
              optimization_guide::proto::Action::kForward);
  }
}

}  // namespace

}  // namespace actor
