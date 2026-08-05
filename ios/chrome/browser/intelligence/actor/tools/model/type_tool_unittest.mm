// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/type_tool.h"

#import <memory>
#import <utility>

#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/type_tool_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

class TypeToolTest : public PlatformTest {
 public:
  TypeToolTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;

  base::expected<std::unique_ptr<TypeTool>, ToolExecutionResult>
  CreateToolAndValidate(const optimization_guide::proto::TypeAction& action,
                        web::WebState* web_state) {
    std::unique_ptr<TypeTool> tool =
        TypeTool::Create(web_state ? web_state->GetWeakPtr() : nullptr, action);
    CHECK(tool);
    base::test::TestFuture<ToolExecutionResult> validate_future;
    tool->Validate(validate_future.GetCallback());
    if (!validate_future.Get().IsOk()) {
      return base::unexpected(validate_future.Get());
    }
    return tool;
  }
};

TEST_F(TypeToolTest, Validate_MissingText) {
  optimization_guide::proto::Action action;
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetBrowserState(profile_.get());
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  action.mutable_type()->set_tab_id(tab_id);
  action.mutable_type()->set_mode(
      optimization_guide::proto::TypeAction::APPEND);
  action.mutable_type()->mutable_target()->set_content_node_id(123);

  base::expected<std::unique_ptr<TypeTool>, ToolExecutionResult> result =
      CreateToolAndValidate(action.type(), web_state_ptr);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kArgumentsInvalid);
}

TEST_F(TypeToolTest, Validate_MissingMode) {
  optimization_guide::proto::Action action;
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetBrowserState(profile_.get());
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  action.mutable_type()->set_tab_id(tab_id);
  action.mutable_type()->set_text("test");
  action.mutable_type()->mutable_target()->set_content_node_id(123);

  base::expected<std::unique_ptr<TypeTool>, ToolExecutionResult> result =
      CreateToolAndValidate(action.type(), web_state_ptr);

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kArgumentsInvalid);
}

TEST_F(TypeToolTest, Validate_MissingTarget) {
  optimization_guide::proto::Action action;
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetBrowserState(profile_.get());
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  action.mutable_type()->set_tab_id(tab_id);
  action.mutable_type()->set_text("test");
  action.mutable_type()->set_mode(
      optimization_guide::proto::TypeAction::APPEND);

  base::expected<std::unique_ptr<TypeTool>, ToolExecutionResult> result =
      CreateToolAndValidate(action.type(), web_state_ptr);

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kArgumentsInvalid);
}



TEST_F(TypeToolTest, Execute_WebStateDestroyed_ReturnsError) {
  auto web_state = std::make_unique<web::FakeWebState>();
  int web_state_index = browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* inserted_web_state =
      browser_->GetWebStateList()->GetWebStateAt(web_state_index);

  optimization_guide::proto::Action action;
  optimization_guide::proto::TypeAction* type_action = action.mutable_type();
  type_action->set_tab_id(
      inserted_web_state->GetUniqueIdentifier().identifier());
  type_action->mutable_target()->mutable_coordinate()->set_x(50);
  type_action->mutable_target()->mutable_coordinate()->set_y(50);
  type_action->set_text("test");
  type_action->set_mode(optimization_guide::proto::TypeAction::APPEND);

  base::expected<std::unique_ptr<TypeTool>, ToolExecutionResult> create_result =
      CreateToolAndValidate(action.type(), inserted_web_state);
  ASSERT_TRUE(create_result.has_value());
  std::unique_ptr<TypeTool> tool = std::move(create_result.value());

  browser_->GetWebStateList()->CloseWebStateAt(
      web_state_index, WebStateList::ClosingReason::kDefault);
  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kTabWentAway);
}

TEST_F(TypeToolTest, Execute_NoWebFramesManager_ReturnsError) {
  auto web_state = std::make_unique<web::FakeWebState>();
  ASSERT_EQ(
      web_state->GetWebFramesManager(
          TypeToolJavaScriptFeature::GetInstance()->GetSupportedContentWorld()),
      nullptr);
  int web_state_index = browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* inserted_web_state =
      browser_->GetWebStateList()->GetWebStateAt(web_state_index);

  optimization_guide::proto::Action action;
  optimization_guide::proto::TypeAction* type_action = action.mutable_type();
  type_action->set_tab_id(
      inserted_web_state->GetUniqueIdentifier().identifier());
  type_action->mutable_target()->mutable_coordinate()->set_x(50);
  type_action->mutable_target()->mutable_coordinate()->set_y(50);
  type_action->set_text("test");
  type_action->set_mode(optimization_guide::proto::TypeAction::APPEND);

  base::expected<std::unique_ptr<TypeTool>, ToolExecutionResult> create_result =
      CreateToolAndValidate(action.type(), inserted_web_state);
  ASSERT_TRUE(create_result.has_value());
  std::unique_ptr<TypeTool> tool = std::move(create_result.value());

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kFrameWentAway);
}

TEST_F(TypeToolTest, Execute_NoMainFrame_ReturnsError) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetWebFramesManager(
      TypeToolJavaScriptFeature::GetInstance()->GetSupportedContentWorld(),
      std::make_unique<web::FakeWebFramesManager>());
  ASSERT_NE(
      web_state->GetWebFramesManager(
          TypeToolJavaScriptFeature::GetInstance()->GetSupportedContentWorld()),
      nullptr);
  ASSERT_EQ(web_state
                ->GetWebFramesManager(TypeToolJavaScriptFeature::GetInstance()
                                          ->GetSupportedContentWorld())
                ->GetMainWebFrame(),
            nullptr);

  int web_state_index = browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* inserted_web_state =
      browser_->GetWebStateList()->GetWebStateAt(web_state_index);

  optimization_guide::proto::Action action;
  optimization_guide::proto::TypeAction* type_action = action.mutable_type();
  type_action->set_tab_id(
      inserted_web_state->GetUniqueIdentifier().identifier());
  type_action->mutable_target()->mutable_coordinate()->set_x(50);
  type_action->mutable_target()->mutable_coordinate()->set_y(50);
  type_action->set_text("test");
  type_action->set_mode(optimization_guide::proto::TypeAction::APPEND);

  base::expected<std::unique_ptr<TypeTool>, ToolExecutionResult> create_result =
      CreateToolAndValidate(action.type(), inserted_web_state);
  ASSERT_TRUE(create_result.has_value());
  std::unique_ptr<TypeTool> tool = std::move(create_result.value());

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kFrameWentAway);
}

TEST_F(TypeToolTest, GetToolType) {
  optimization_guide::proto::Action action;
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetBrowserState(profile_.get());
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  action.mutable_type()->set_tab_id(tab_id);
  action.mutable_type()->set_text("test");
  action.mutable_type()->set_mode(
      optimization_guide::proto::TypeAction::APPEND);
  action.mutable_type()->mutable_target()->mutable_coordinate()->set_x(50);
  action.mutable_type()->mutable_target()->mutable_coordinate()->set_y(50);

  base::expected<std::unique_ptr<TypeTool>, ToolExecutionResult> result =
      CreateToolAndValidate(action.type(), web_state_ptr);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value()->GetToolType(), ToolType::kType);
}

}  // namespace actor
