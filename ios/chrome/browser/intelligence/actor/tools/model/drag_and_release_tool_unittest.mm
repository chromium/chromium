// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/drag_and_release_tool.h"

#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/unguessable_token.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/drag_and_release_tool_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace actor {

// Test fixture for DragAndReleaseTool.
class DragAndReleaseToolTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile());
    BrowserList* browser_list = BrowserListFactory::GetForProfile(profile());
    browser_list->AddBrowser(browser_.get());

    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_ = web_state.get();
    tab_id_ = web_state->GetUniqueIdentifier().identifier();
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());
  }

  void TearDown() override {
    web_state_ = nullptr;
    browser_.reset();
    profile_.reset();
    PlatformTest::TearDown();
  }

  TestProfileIOS* profile() const { return profile_.get(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<web::WebState> web_state_ = nullptr;
  int32_t tab_id_ = 0;

  base::expected<std::unique_ptr<DragAndReleaseTool>, ToolExecutionResult>
  CreateToolAndValidate(
      const optimization_guide::proto::DragAndReleaseAction& action,
      web::WebState* web_state) {
    std::unique_ptr<DragAndReleaseTool> tool = DragAndReleaseTool::Create(
        web_state ? web_state->GetWeakPtr() : nullptr, action);
    if (!tool) {
      return base::unexpected(
          ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    }
    base::test::TestFuture<ToolExecutionResult> validate_future;
    tool->Validate(validate_future.GetCallback());
    if (!validate_future.Get().IsOk()) {
      return base::unexpected(validate_future.Get());
    }
    return tool;
  }
};

// Tests that validation fails when the source target is missing coordinates and
// node IDs.
TEST_F(DragAndReleaseToolTest, Validate_MissingFromTarget) {
  optimization_guide::proto::DragAndReleaseAction action;
  action.set_tab_id(tab_id_);
  action.mutable_to_target()->mutable_coordinate()->set_x(10);
  action.mutable_to_target()->mutable_coordinate()->set_y(10);

  base::expected<std::unique_ptr<DragAndReleaseTool>, ToolExecutionResult>
      result = CreateToolAndValidate(action, web_state_);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kArgumentsInvalid);
}

// Tests that validation fails when the destination target is missing
// coordinates and node IDs.
TEST_F(DragAndReleaseToolTest, Validate_MissingToTarget) {
  optimization_guide::proto::DragAndReleaseAction action;
  action.set_tab_id(tab_id_);
  action.mutable_from_target()->mutable_coordinate()->set_x(1);
  action.mutable_from_target()->mutable_coordinate()->set_y(1);

  base::expected<std::unique_ptr<DragAndReleaseTool>, ToolExecutionResult>
      result = CreateToolAndValidate(action, web_state_);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kArgumentsInvalid);
}

// Tests that validation succeeds when both from and to targets are valid.
TEST_F(DragAndReleaseToolTest, Validate_Success) {
  optimization_guide::proto::DragAndReleaseAction action;
  action.set_tab_id(tab_id_);
  action.mutable_from_target()->mutable_coordinate()->set_x(1);
  action.mutable_from_target()->mutable_coordinate()->set_y(1);
  action.mutable_to_target()->mutable_coordinate()->set_x(10);
  action.mutable_to_target()->mutable_coordinate()->set_y(10);

  base::expected<std::unique_ptr<DragAndReleaseTool>, ToolExecutionResult>
      result = CreateToolAndValidate(action, web_state_);
  EXPECT_TRUE(result.has_value());
}

// Tests that execution returns kTabWentAway when the target WebState is
// destroyed.
TEST_F(DragAndReleaseToolTest, Execute_WebStateDestroyed_ReturnsError) {
  optimization_guide::proto::DragAndReleaseAction action;
  action.set_tab_id(tab_id_);
  action.mutable_from_target()->mutable_coordinate()->set_x(1);
  action.mutable_from_target()->mutable_coordinate()->set_y(1);
  action.mutable_to_target()->mutable_coordinate()->set_x(10);
  action.mutable_to_target()->mutable_coordinate()->set_y(10);

  base::expected<std::unique_ptr<DragAndReleaseTool>, ToolExecutionResult>
      create_result = CreateToolAndValidate(action, web_state_);
  ASSERT_TRUE(create_result.has_value());
  std::unique_ptr<DragAndReleaseTool> tool = std::move(create_result.value());

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

// Tests that execution returns kFrameWentAway when the WebFramesManager is
// null.
TEST_F(DragAndReleaseToolTest, Execute_NoWebFramesManager_ReturnsError) {
  auto web_state = std::make_unique<web::FakeWebState>();
  ASSERT_EQ(web_state->GetWebFramesManager(
                DragAndReleaseToolJavaScriptFeature::GetInstance()
                    ->GetSupportedContentWorld()),
            nullptr);
  int web_state_index = browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* inserted_web_state =
      browser_->GetWebStateList()->GetWebStateAt(web_state_index);

  optimization_guide::proto::DragAndReleaseAction action;
  action.set_tab_id(inserted_web_state->GetUniqueIdentifier().identifier());
  action.mutable_from_target()->mutable_coordinate()->set_x(1);
  action.mutable_from_target()->mutable_coordinate()->set_y(1);
  action.mutable_to_target()->mutable_coordinate()->set_x(10);
  action.mutable_to_target()->mutable_coordinate()->set_y(10);

  base::expected<std::unique_ptr<DragAndReleaseTool>, ToolExecutionResult>
      create_result = CreateToolAndValidate(action, inserted_web_state);
  ASSERT_TRUE(create_result.has_value());
  std::unique_ptr<DragAndReleaseTool> tool = std::move(create_result.value());

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kFrameWentAway);
}

// Tests that execution returns kFrameWentAway when the main WebFrame is null.
TEST_F(DragAndReleaseToolTest, Execute_NoMainFrame_ReturnsError) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetWebFramesManager(
      DragAndReleaseToolJavaScriptFeature::GetInstance()
          ->GetSupportedContentWorld(),
      std::make_unique<web::FakeWebFramesManager>());
  ASSERT_NE(web_state->GetWebFramesManager(
                DragAndReleaseToolJavaScriptFeature::GetInstance()
                    ->GetSupportedContentWorld()),
            nullptr);
  ASSERT_EQ(web_state
                ->GetWebFramesManager(
                    DragAndReleaseToolJavaScriptFeature::GetInstance()
                        ->GetSupportedContentWorld())
                ->GetMainWebFrame(),
            nullptr);

  int web_state_index = browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* inserted_web_state =
      browser_->GetWebStateList()->GetWebStateAt(web_state_index);

  optimization_guide::proto::DragAndReleaseAction action;
  action.set_tab_id(inserted_web_state->GetUniqueIdentifier().identifier());
  action.mutable_from_target()->mutable_coordinate()->set_x(1);
  action.mutable_from_target()->mutable_coordinate()->set_y(1);
  action.mutable_to_target()->mutable_coordinate()->set_x(10);
  action.mutable_to_target()->mutable_coordinate()->set_y(10);

  base::expected<std::unique_ptr<DragAndReleaseTool>, ToolExecutionResult>
      create_result = CreateToolAndValidate(action, inserted_web_state);
  ASSERT_TRUE(create_result.has_value());
  std::unique_ptr<DragAndReleaseTool> tool = std::move(create_result.value());

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kFrameWentAway);
}

// Tests that execution returns `kNotImplemented` when `from_target` and
// `to_target` resolve to different frames.
TEST_F(DragAndReleaseToolTest, Execute_CrossFrame_ReturnsNotImplemented) {
  auto web_state = std::make_unique<web::FakeWebState>();
  auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
  std::unique_ptr<web::FakeWebFrame> main_frame =
      web::FakeWebFrame::CreateMainWebFrame(GURL("https://example.com"));
  std::unique_ptr<web::FakeWebFrame> child_frame =
      web::FakeWebFrame::CreateChildWebFrame(GURL("https://example.com/child"));

  std::string main_frame_id = main_frame->GetFrameId();
  std::string child_frame_id = child_frame->GetFrameId();
  frames_manager->AddWebFrame(std::move(main_frame));
  frames_manager->AddWebFrame(std::move(child_frame));
  web_state->SetWebFramesManager(
      DragAndReleaseToolJavaScriptFeature::GetInstance()
          ->GetSupportedContentWorld(),
      std::move(frames_manager));
  web_state->SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                 std::make_unique<web::FakeWebFramesManager>());

  base::UnguessableToken main_remote_token = base::UnguessableToken::Create();
  base::UnguessableToken child_remote_token = base::UnguessableToken::Create();
  std::optional<base::UnguessableToken> main_local_token =
      autofill::DeserializeJavaScriptFrameId(main_frame_id);
  std::optional<base::UnguessableToken> child_local_token =
      autofill::DeserializeJavaScriptFrameId(child_frame_id);
  ASSERT_TRUE(main_local_token.has_value());
  ASSERT_TRUE(child_local_token.has_value());

  autofill::ChildFrameRegistrar* registrar =
      autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state.get());
  registrar->RegisterMapping(autofill::RemoteFrameToken(main_remote_token),
                             autofill::LocalFrameToken(*main_local_token));
  registrar->RegisterMapping(autofill::RemoteFrameToken(child_remote_token),
                             autofill::LocalFrameToken(*child_local_token));

  int web_state_index = browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* inserted_web_state =
      browser_->GetWebStateList()->GetWebStateAt(web_state_index);

  optimization_guide::proto::DragAndReleaseAction action;
  action.set_tab_id(inserted_web_state->GetUniqueIdentifier().identifier());
  action.mutable_from_target()->set_content_node_id(1);
  action.mutable_from_target()
      ->mutable_document_identifier()
      ->set_serialized_token(main_remote_token.ToString());
  action.mutable_to_target()->set_content_node_id(2);
  action.mutable_to_target()
      ->mutable_document_identifier()
      ->set_serialized_token(child_remote_token.ToString());

  base::expected<std::unique_ptr<DragAndReleaseTool>, ToolExecutionResult>
      create_result = CreateToolAndValidate(action, inserted_web_state);
  ASSERT_TRUE(create_result.has_value());
  std::unique_ptr<DragAndReleaseTool> tool = std::move(create_result.value());

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kNotImplemented);
  EXPECT_FALSE(result.requires_page_stabilization());
  EXPECT_EQ(result.message(), "Cross-frame drag and release is not supported.");
}

// Tests that GetTargetWebState returns the associated WebState.
TEST_F(DragAndReleaseToolTest, GetTargetWebState) {
  optimization_guide::proto::DragAndReleaseAction action;
  action.set_tab_id(tab_id_);
  action.mutable_from_target()->mutable_coordinate()->set_x(1);
  action.mutable_from_target()->mutable_coordinate()->set_y(1);
  action.mutable_to_target()->mutable_coordinate()->set_x(10);
  action.mutable_to_target()->mutable_coordinate()->set_y(10);

  base::expected<std::unique_ptr<DragAndReleaseTool>, ToolExecutionResult>
      result = CreateToolAndValidate(action, web_state_);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value()->GetTargetWebState().get(), web_state_);
}

// Tests that GetToolType returns ToolType::kDragAndRelease.
TEST_F(DragAndReleaseToolTest, GetToolType) {
  optimization_guide::proto::DragAndReleaseAction action;
  action.set_tab_id(tab_id_);
  action.mutable_from_target()->mutable_coordinate()->set_x(1);
  action.mutable_from_target()->mutable_coordinate()->set_y(1);
  action.mutable_to_target()->mutable_coordinate()->set_x(10);
  action.mutable_to_target()->mutable_coordinate()->set_y(10);

  base::expected<std::unique_ptr<DragAndReleaseTool>, ToolExecutionResult>
      result = CreateToolAndValidate(action, web_state_);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value()->GetToolType(), ToolType::kDragAndRelease);
}

}  // namespace actor
