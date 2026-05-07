// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/select_tool.h"

#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/select_tool_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

class SelectToolTest : public PlatformTest {
 protected:
  void SetUp() override {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile());
    BrowserList* browser_list = BrowserListFactory::GetForProfile(profile());
    browser_list->AddBrowser(browser_.get());

    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_ = web_state.get();
    tab_id_ = web_state->GetUniqueIdentifier().identifier();
    browser_->GetWebStateList()->InsertWebState(std::move(web_state));
  }

  TestProfileIOS* profile() const { return profile_.get(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<web::WebState> web_state_;
  int32_t tab_id_;
};

TEST_F(SelectToolTest, Create_MissingTabId) {
  optimization_guide::proto::SelectAction action;
  action.set_value("v1");
  action.mutable_target()->mutable_coordinate()->set_x(1);
  action.mutable_target()->mutable_coordinate()->set_y(1);

  auto result = SelectTool::Create(action, profile());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kArgumentsInvalid);
}

TEST_F(SelectToolTest, Create_NoWebStateForTabId) {
  optimization_guide::proto::SelectAction action;
  action.set_tab_id(1);
  action.set_value("v1");
  action.mutable_target()->mutable_coordinate()->set_x(1);
  action.mutable_target()->mutable_coordinate()->set_y(1);

  base::expected<std::unique_ptr<SelectTool>, ToolExecutionResult> result =
      SelectTool::Create(action, profile_.get());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kTabWentAway);
}

TEST_F(SelectToolTest, Create_MissingValueField) {
  optimization_guide::proto::SelectAction action;
  action.set_tab_id(tab_id_);
  action.mutable_target()->mutable_coordinate()->set_x(1);
  action.mutable_target()->mutable_coordinate()->set_y(1);

  auto result = SelectTool::Create(action, profile());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kArgumentsInvalid);
}

TEST_F(SelectToolTest, Create_MissingTarget) {
  optimization_guide::proto::SelectAction action;
  action.set_tab_id(tab_id_);
  action.set_value("v1");

  auto result = SelectTool::Create(action, profile());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kArgumentsInvalid);
}

TEST_F(SelectToolTest, Create_ByCoordinates_Success) {
  optimization_guide::proto::SelectAction action;
  action.set_tab_id(tab_id_);
  action.set_value("v1");
  action.mutable_target()->mutable_coordinate()->set_x(1);
  action.mutable_target()->mutable_coordinate()->set_y(1);

  auto result = SelectTool::Create(action, profile());
  EXPECT_TRUE(result.has_value());
}

TEST_F(SelectToolTest, Create_ByIdentifiers_Success) {
  optimization_guide::proto::SelectAction action;
  action.set_tab_id(tab_id_);
  action.set_value("v1");
  action.mutable_target()->set_content_node_id(1);
  action.mutable_target()->mutable_document_identifier()->set_serialized_token(
      "fake_id");

  auto result = SelectTool::Create(action, profile());
  EXPECT_TRUE(result.has_value());
}

TEST_F(SelectToolTest, Create_NodeIdWithoutDocumentIdentifier_Invalid) {
  optimization_guide::proto::SelectAction action;
  action.set_tab_id(tab_id_);
  action.set_value("v1");

  auto* target = action.mutable_target();
  target->set_content_node_id(1);
  // Omit document_identifier

  auto result = SelectTool::Create(action, profile());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kArgumentsInvalid);
}

TEST_F(SelectToolTest, Create_BothTargetingTypes_Invalid) {
  optimization_guide::proto::SelectAction action;
  action.set_tab_id(tab_id_);
  action.set_value("v1");

  auto* target = action.mutable_target();
  target->mutable_coordinate()->set_x(1);
  target->mutable_coordinate()->set_y(1);
  target->set_content_node_id(1);
  target->mutable_document_identifier()->set_serialized_token("fake_id");

  auto result = SelectTool::Create(action, profile());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kArgumentsInvalid);
}

TEST_F(SelectToolTest, Execute_WebStateDestroyed_ReturnsError) {
  optimization_guide::proto::SelectAction select_action;
  select_action.set_tab_id(tab_id_);
  select_action.mutable_target()->mutable_coordinate()->set_x(1);
  select_action.mutable_target()->mutable_coordinate()->set_y(1);
  select_action.set_value("v1");

  auto create_result = SelectTool::Create(select_action, profile_.get());
  ASSERT_TRUE(create_result.has_value());
  std::unique_ptr<SelectTool> tool = std::move(create_result.value());

  int index_to_close =
      browser_->GetWebStateList()->GetIndexOfWebState(web_state_);
  web_state_ = nullptr;
  browser_->GetWebStateList()->CloseWebStateAt(
      index_to_close, WebStateList::ClosingReason::kDefault);
  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kTabWentAway);
}

TEST_F(SelectToolTest, Execute_NoWebFramesManager_ReturnsError) {
  auto web_state = std::make_unique<web::FakeWebState>();
  ASSERT_EQ(
      web_state->GetWebFramesManager(SelectToolJavaScriptFeature::GetInstance()
                                         ->GetSupportedContentWorld()),
      nullptr);
  int web_state_index = browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* inserted_web_state =
      browser_->GetWebStateList()->GetWebStateAt(web_state_index);

  optimization_guide::proto::SelectAction select_action;
  select_action.set_tab_id(
      inserted_web_state->GetUniqueIdentifier().identifier());
  select_action.mutable_target()->mutable_coordinate()->set_x(1);
  select_action.mutable_target()->mutable_coordinate()->set_y(1);
  select_action.set_value("v1");

  auto create_result = SelectTool::Create(select_action, profile_.get());
  ASSERT_TRUE(create_result.has_value());
  std::unique_ptr<SelectTool> tool = std::move(create_result.value());

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kFrameWentAway);
}

TEST_F(SelectToolTest, Execute_NoMainFrame_ReturnsError) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetWebFramesManager(
      SelectToolJavaScriptFeature::GetInstance()->GetSupportedContentWorld(),
      std::make_unique<web::FakeWebFramesManager>());
  ASSERT_NE(
      web_state->GetWebFramesManager(SelectToolJavaScriptFeature::GetInstance()
                                         ->GetSupportedContentWorld()),
      nullptr);
  ASSERT_EQ(web_state
                ->GetWebFramesManager(SelectToolJavaScriptFeature::GetInstance()
                                          ->GetSupportedContentWorld())
                ->GetMainWebFrame(),
            nullptr);

  int web_state_index = browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* inserted_web_state =
      browser_->GetWebStateList()->GetWebStateAt(web_state_index);

  optimization_guide::proto::SelectAction select_action;
  select_action.set_tab_id(
      inserted_web_state->GetUniqueIdentifier().identifier());
  select_action.mutable_target()->mutable_coordinate()->set_x(1);
  select_action.mutable_target()->mutable_coordinate()->set_y(1);
  select_action.set_value("v1");

  auto create_result = SelectTool::Create(select_action, profile_.get());
  ASSERT_TRUE(create_result.has_value());
  std::unique_ptr<SelectTool> tool = std::move(create_result.value());

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kFrameWentAway);
}

TEST_F(SelectToolTest, GetActionCase) {
  optimization_guide::proto::SelectAction action;
  action.set_tab_id(tab_id_);
  action.set_value("v1");
  action.mutable_target()->mutable_coordinate()->set_x(1);
  action.mutable_target()->mutable_coordinate()->set_y(1);

  auto result = SelectTool::Create(action, profile());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value()->GetActionCase(),
            optimization_guide::proto::Action::kSelect);
}

}  // namespace actor
