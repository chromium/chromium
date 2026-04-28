// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/click_tool_java_script_feature.h"

#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_test_base.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "testing/gtest/include/gtest/gtest.h"

using optimization_guide::proto::ClickAction;

namespace actor {

class ClickToolJavaScriptFeatureTest
    : public ActorToolJavaScriptFeatureTestBase {
 protected:
  ClickToolJavaScriptFeatureTest() : ActorToolJavaScriptFeatureTestBase() {}

  void SetUp() override { ActorToolJavaScriptFeatureTestBase::SetUp(); }

  ClickToolJavaScriptFeature* feature() {
    return ClickToolJavaScriptFeature::GetInstance();
  }

  // Mocks both JavaScript functions for clicking to return the given result.
  void MockClickJsFunctions(const std::string& mock_return_value) {
    MockJsFunction(feature(), "click_tool", "clickByCoordinate",
                   mock_return_value);
    MockJsFunction(feature(), "click_tool", "clickByNodeId", mock_return_value);
  }

  ClickAction CreateClickActionWithCoordinates() {
    ClickAction action;
    // Use arbitrary values since the JS function is mocked.
    action.mutable_target()->mutable_coordinate()->set_x(1);
    action.mutable_target()->mutable_coordinate()->set_y(2);
    action.mutable_target()->mutable_coordinate()->set_pixel_type(
        optimization_guide::proto::Coordinate::PIXEL_TYPE_UNSPECIFIED);
    action.set_click_type(ClickAction::UNKNOWN_CLICK_TYPE);
    action.set_click_count(ClickAction::UNKNOWN_CLICK_COUNT);
    return action;
  }

  ClickAction CreateClickActionWithNodeId() {
    ClickAction action;
    // Use arbitrary values since the JS function is mocked.
    action.mutable_target()->set_content_node_id(123);
    action.mutable_target()
        ->mutable_document_identifier()
        ->set_serialized_token("doc_id");
    action.set_click_type(ClickAction::UNKNOWN_CLICK_TYPE);
    action.set_click_count(ClickAction::UNKNOWN_CLICK_COUNT);
    return action;
  }
};

TEST_F(ClickToolJavaScriptFeatureTest, JsReturnsNonDict) {
  MockClickJsFunctions(/*mock_return_value=*/"'unexpected type'");
  ClickAction click_by_coordinate = CreateClickActionWithCoordinates();
  ClickAction click_by_node_id = CreateClickActionWithNodeId();
  base::test::TestFuture<ToolExecutionResult> coordinate_future;
  base::test::TestFuture<ToolExecutionResult> node_id_future;

  feature()->Click(GetMainFrame(feature()), click_by_coordinate,
                   coordinate_future.GetCallback());
  feature()->Click(GetMainFrame(feature()), click_by_node_id,
                   node_id_future.GetCallback());

  auto coordinate_result = coordinate_future.Get();
  EXPECT_FALSE(coordinate_result.IsOk());
  EXPECT_EQ(coordinate_result.internal_code().value(),
            InternalToolErrorCode::kJavascriptFeatureGotInvalidResult);

  auto node_id_result = node_id_future.Get();
  EXPECT_FALSE(node_id_result.IsOk());
  EXPECT_EQ(node_id_result.internal_code().value(),
            InternalToolErrorCode::kJavascriptFeatureGotInvalidResult);
}

TEST_F(ClickToolJavaScriptFeatureTest, JsReturnsError) {
  MockClickJsFunctions(
      /*mock_return_value=*/"{success: false, message: 'Custom JS Error'}");
  ClickAction click_by_coordinate = CreateClickActionWithCoordinates();
  ClickAction click_by_node_id = CreateClickActionWithNodeId();
  base::test::TestFuture<ToolExecutionResult> coordinate_future;
  base::test::TestFuture<ToolExecutionResult> node_id_future;

  feature()->Click(GetMainFrame(feature()), click_by_coordinate,
                   coordinate_future.GetCallback());
  feature()->Click(GetMainFrame(feature()), click_by_node_id,
                   node_id_future.GetCallback());

  auto coordinate_result = coordinate_future.Get();
  EXPECT_FALSE(coordinate_result.IsOk());
  EXPECT_EQ(
      coordinate_result.internal_code().value(),
      InternalToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution);
  EXPECT_EQ(coordinate_result.message().value(), "Custom JS Error");

  auto node_id_result = node_id_future.Get();
  EXPECT_FALSE(node_id_result.IsOk());
  EXPECT_EQ(
      node_id_result.internal_code().value(),
      InternalToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution);
  EXPECT_EQ(node_id_result.message().value(), "Custom JS Error");
}

TEST_F(ClickToolJavaScriptFeatureTest, InvalidatedWebFrame) {
  ClickAction type_by_coordinate = CreateClickActionWithCoordinates();
  ClickAction type_by_node_id = CreateClickActionWithNodeId();
  base::test::TestFuture<ToolExecutionResult> coordinate_future;
  base::test::TestFuture<ToolExecutionResult> node_id_future;

  feature()->Click(/*target_frame=*/nullptr, type_by_coordinate,
                   coordinate_future.GetCallback());
  feature()->Click(/*target_frame=*/nullptr, type_by_node_id,
                   node_id_future.GetCallback());

  auto coordinate_result = coordinate_future.Get();
  EXPECT_FALSE(coordinate_result.IsOk());
  EXPECT_EQ(coordinate_result.internal_code().value(),
            InternalToolErrorCode::kActorTargetWebFrameInvalidated);
  auto node_id_result = node_id_future.Get();
  EXPECT_FALSE(node_id_result.IsOk());
  EXPECT_EQ(node_id_result.internal_code().value(),
            InternalToolErrorCode::kActorTargetWebFrameInvalidated);
}

TEST_F(ClickToolJavaScriptFeatureTest, JsReturnsErrorWithoutMessage) {
  MockClickJsFunctions(/*mock_return_value=*/"{success: false}");
  ClickAction click_by_coordinate = CreateClickActionWithCoordinates();
  ClickAction click_by_node_id = CreateClickActionWithNodeId();
  base::test::TestFuture<ToolExecutionResult> coordinate_future;
  base::test::TestFuture<ToolExecutionResult> node_id_future;

  feature()->Click(GetMainFrame(feature()), click_by_coordinate,
                   coordinate_future.GetCallback());
  feature()->Click(GetMainFrame(feature()), click_by_node_id,
                   node_id_future.GetCallback());

  auto coordinate_result = coordinate_future.Get();
  EXPECT_FALSE(coordinate_result.IsOk());
  EXPECT_EQ(
      coordinate_result.internal_code().value(),
      InternalToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution);
  EXPECT_EQ(coordinate_result.message().value(), "Unknown error in JS.");

  auto node_id_result = node_id_future.Get();
  EXPECT_FALSE(node_id_result.IsOk());
  EXPECT_EQ(
      node_id_result.internal_code().value(),
      InternalToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution);
  EXPECT_EQ(node_id_result.message().value(), "Unknown error in JS.");
}

TEST_F(ClickToolJavaScriptFeatureTest, ClickByCoordinate_Success) {
  MockClickJsFunctions(
      /*mock_return_value=*/"{success: true, message: 'fake success!'}");
  ClickAction action = CreateClickActionWithCoordinates();
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->Click(GetMainFrame(feature()), action, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.IsOk());
}

TEST_F(ClickToolJavaScriptFeatureTest, ClickByNodeId_Success) {
  MockClickJsFunctions(
      /*mock_return_value=*/"{success: true, message: 'fake success!'}");
  ClickAction action = CreateClickActionWithNodeId();
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->Click(GetMainFrame(feature()), action, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.IsOk());
}

}  // namespace actor
