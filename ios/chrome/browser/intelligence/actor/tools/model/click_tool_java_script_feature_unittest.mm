// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/click_tool_java_script_feature.h"

#import "base/strings/stringprintf.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
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

  ActionTarget CreateTargetWithCoordinates() {
    optimization_guide::proto::ActionTarget target;
    // Use arbitrary values since the JS function is mocked.
    target.mutable_coordinate()->set_x(1);
    target.mutable_coordinate()->set_y(2);
    target.mutable_coordinate()->set_pixel_type(
        optimization_guide::proto::Coordinate::PIXEL_TYPE_UNSPECIFIED);
    return ActionTarget::FromProto(target);
  }

  ActionTarget CreateTargetWithNodeId() {
    optimization_guide::proto::ActionTarget target;
    // Use arbitrary values since the JS function is mocked.
    target.set_content_node_id(123);
    target.mutable_document_identifier()->set_serialized_token("doc_id");
    return ActionTarget::FromProto(target);
  }
};

TEST_F(ClickToolJavaScriptFeatureTest, JsReturnsNonDict) {
  MockClickJsFunctions(/*mock_return_value=*/"'unexpected type'");
  ActionTarget click_by_coordinate = CreateTargetWithCoordinates();
  ActionTarget click_by_node_id = CreateTargetWithNodeId();
  base::test::TestFuture<ToolExecutionResult> coordinate_future;
  base::test::TestFuture<ToolExecutionResult> node_id_future;

  feature()->Click(GetMainFrame(feature()), click_by_coordinate,
                   ClickAction::UNKNOWN_CLICK_TYPE,
                   ClickAction::UNKNOWN_CLICK_COUNT,
                   coordinate_future.GetCallback());
  feature()->Click(GetMainFrame(feature()), click_by_node_id,
                   ClickAction::UNKNOWN_CLICK_TYPE,
                   ClickAction::UNKNOWN_CLICK_COUNT,
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
      /*mock_return_value=*/base::StringPrintf(
          "{resultCode: %d, message: 'Custom JS Error'}",
          static_cast<int>(ClickToolResultCode::kClickSuppressed)));
  ActionTarget click_by_coordinate = CreateTargetWithCoordinates();
  ActionTarget click_by_node_id = CreateTargetWithNodeId();
  base::test::TestFuture<ToolExecutionResult> coordinate_future;
  base::test::TestFuture<ToolExecutionResult> node_id_future;

  feature()->Click(GetMainFrame(feature()), click_by_coordinate,
                   ClickAction::UNKNOWN_CLICK_TYPE,
                   ClickAction::UNKNOWN_CLICK_COUNT,
                   coordinate_future.GetCallback());
  feature()->Click(GetMainFrame(feature()), click_by_node_id,
                   ClickAction::UNKNOWN_CLICK_TYPE,
                   ClickAction::UNKNOWN_CLICK_COUNT,
                   node_id_future.GetCallback());

  auto coordinate_result = coordinate_future.Get();
  EXPECT_FALSE(coordinate_result.IsOk());
  EXPECT_EQ(coordinate_result.code(),
            mojom::ActionResultCode::kClickSuppressed);
  EXPECT_EQ(GetToolExecutionResultMessage(coordinate_result),
            "Custom JS Error");

  auto node_id_result = node_id_future.Get();
  EXPECT_FALSE(node_id_result.IsOk());
  EXPECT_EQ(node_id_result.code(), mojom::ActionResultCode::kClickSuppressed);
  EXPECT_EQ(GetToolExecutionResultMessage(node_id_result), "Custom JS Error");
}

TEST_F(ClickToolJavaScriptFeatureTest, InvalidatedWebFrame) {
  ActionTarget type_by_coordinate = CreateTargetWithCoordinates();
  ActionTarget type_by_node_id = CreateTargetWithNodeId();
  base::test::TestFuture<ToolExecutionResult> coordinate_future;
  base::test::TestFuture<ToolExecutionResult> node_id_future;

  feature()->Click(/*target_frame=*/nullptr, type_by_coordinate,
                   ClickAction::UNKNOWN_CLICK_TYPE,
                   ClickAction::UNKNOWN_CLICK_COUNT,
                   coordinate_future.GetCallback());
  feature()->Click(/*target_frame=*/nullptr, type_by_node_id,
                   ClickAction::UNKNOWN_CLICK_TYPE,
                   ClickAction::UNKNOWN_CLICK_COUNT,
                   node_id_future.GetCallback());

  auto coordinate_result = coordinate_future.Get();
  EXPECT_FALSE(coordinate_result.IsOk());
  EXPECT_EQ(coordinate_result.code(), mojom::ActionResultCode::kFrameWentAway);
  auto node_id_result = node_id_future.Get();
  EXPECT_FALSE(node_id_result.IsOk());
  EXPECT_EQ(node_id_result.code(), mojom::ActionResultCode::kFrameWentAway);
}

TEST_F(ClickToolJavaScriptFeatureTest, JsReturnsErrorWithoutMessage) {
  MockClickJsFunctions(
      /*mock_return_value=*/base::StringPrintf(
          "{resultCode: %d}",
          static_cast<int>(ClickToolResultCode::kClickSuppressed)));
  ActionTarget click_by_coordinate = CreateTargetWithCoordinates();
  ActionTarget click_by_node_id = CreateTargetWithNodeId();
  base::test::TestFuture<ToolExecutionResult> coordinate_future;
  base::test::TestFuture<ToolExecutionResult> node_id_future;

  feature()->Click(GetMainFrame(feature()), click_by_coordinate,
                   ClickAction::UNKNOWN_CLICK_TYPE,
                   ClickAction::UNKNOWN_CLICK_COUNT,
                   coordinate_future.GetCallback());
  feature()->Click(GetMainFrame(feature()), click_by_node_id,
                   ClickAction::UNKNOWN_CLICK_TYPE,
                   ClickAction::UNKNOWN_CLICK_COUNT,
                   node_id_future.GetCallback());

  auto coordinate_result = coordinate_future.Get();
  EXPECT_FALSE(coordinate_result.IsOk());
  EXPECT_EQ(coordinate_result.code(),
            mojom::ActionResultCode::kClickSuppressed);
  EXPECT_FALSE(coordinate_result.message().has_value());

  auto node_id_result = node_id_future.Get();
  EXPECT_FALSE(node_id_result.IsOk());
  EXPECT_EQ(node_id_result.code(), mojom::ActionResultCode::kClickSuppressed);
  EXPECT_FALSE(node_id_result.message().has_value());
}

TEST_F(ClickToolJavaScriptFeatureTest, ClickByCoordinate_Success) {
  MockClickJsFunctions(
      /*mock_return_value=*/"{resultCode: 0, message: 'fake success!'}");
  ActionTarget action = CreateTargetWithCoordinates();
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->Click(GetMainFrame(feature()), action,
                   ClickAction::UNKNOWN_CLICK_TYPE,
                   ClickAction::UNKNOWN_CLICK_COUNT, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.IsOk());
}

TEST_F(ClickToolJavaScriptFeatureTest, ClickByNodeId_Success) {
  MockClickJsFunctions(
      /*mock_return_value=*/"{resultCode: 0, message: 'fake success!'}");
  ActionTarget action = CreateTargetWithNodeId();
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->Click(GetMainFrame(feature()), action,
                   ClickAction::UNKNOWN_CLICK_TYPE,
                   ClickAction::UNKNOWN_CLICK_COUNT, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.IsOk());
}

}  // namespace actor
