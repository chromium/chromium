// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/type_tool_java_script_feature.h"

#import "base/strings/stringprintf.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_test_base.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "testing/gtest/include/gtest/gtest.h"

using optimization_guide::proto::TypeAction;

namespace actor {

class TypeToolJavaScriptFeatureTest
    : public ActorToolJavaScriptFeatureTestBase {
 protected:
  TypeToolJavaScriptFeatureTest() : ActorToolJavaScriptFeatureTestBase() {}

  void SetUp() override { ActorToolJavaScriptFeatureTestBase::SetUp(); }

  TypeToolJavaScriptFeature* feature() {
    return TypeToolJavaScriptFeature::GetInstance();
  }

  // Mocks both JavaScript functions for typing to return the given result.
  void MockTypeJsFunctions(const std::string& mock_return_value) {
    MockJsFunction(feature(), "type_tool", "typeByCoordinate",
                   mock_return_value);
    MockJsFunction(feature(), "type_tool", "typeByNodeId", mock_return_value);
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

  ActionTarget CreateTargetWithIdentifiers() {
    optimization_guide::proto::ActionTarget target;
    // Use arbitrary values since the JS function is mocked.
    target.set_content_node_id(0);
    target.mutable_document_identifier()->set_serialized_token("token");
    return ActionTarget::FromProto(target);
  }
};

TEST_F(TypeToolJavaScriptFeatureTest, JsReturnsNonDict) {
  MockTypeJsFunctions(/*mock_return_value=*/"[]");
  ActionTarget type_by_coordinate = CreateTargetWithCoordinates();
  ActionTarget type_by_node_id = CreateTargetWithIdentifiers();
  base::test::TestFuture<ToolExecutionResult> coordinate_future;
  base::test::TestFuture<ToolExecutionResult> node_id_future;

  feature()->Type(GetMainFrame(feature()), type_by_coordinate, "default",
                  TypeAction::UNKNOWN_TYPE_MODE, false,
                  coordinate_future.GetCallback());
  feature()->Type(GetMainFrame(feature()), type_by_node_id, "default",
                  TypeAction::UNKNOWN_TYPE_MODE, false,
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

TEST_F(TypeToolJavaScriptFeatureTest, JsReturnsError) {
  TypeToolResultCode js_code = TypeToolResultCode::kTypeTargetNotElement;
  auto expected_code = mojom::ActionResultCode::kTypeTargetNotElement;
  MockTypeJsFunctions(
      /*mock_return_value=*/base::StringPrintf(
          "{resultCode: %d, message: 'Custom JS Error'}", js_code));
  ActionTarget type_by_coordinate = CreateTargetWithCoordinates();
  ActionTarget type_by_node_id = CreateTargetWithIdentifiers();
  base::test::TestFuture<ToolExecutionResult> coordinate_future;
  base::test::TestFuture<ToolExecutionResult> node_id_future;

  feature()->Type(GetMainFrame(feature()), type_by_coordinate, "default",
                  TypeAction::UNKNOWN_TYPE_MODE, false,
                  coordinate_future.GetCallback());
  feature()->Type(GetMainFrame(feature()), type_by_node_id, "default",
                  TypeAction::UNKNOWN_TYPE_MODE, false,
                  node_id_future.GetCallback());

  auto coordinate_result = coordinate_future.Get();
  EXPECT_FALSE(coordinate_result.IsOk());
  EXPECT_EQ(coordinate_result.code(), expected_code);
  EXPECT_EQ(coordinate_result.message().value(), "Custom JS Error");

  auto node_id_result = node_id_future.Get();
  EXPECT_FALSE(node_id_result.IsOk());
  EXPECT_EQ(node_id_result.code(), expected_code);
  EXPECT_EQ(node_id_result.message().value(), "Custom JS Error");
}

TEST_F(TypeToolJavaScriptFeatureTest, InvalidatedWebFrame) {
  ActionTarget type_by_coordinate = CreateTargetWithCoordinates();
  ActionTarget type_by_node_id = CreateTargetWithIdentifiers();
  base::test::TestFuture<ToolExecutionResult> coordinate_future;
  base::test::TestFuture<ToolExecutionResult> node_id_future;

  feature()->Type(/*target_frame=*/nullptr, type_by_coordinate, "default",
                  TypeAction::UNKNOWN_TYPE_MODE, false,
                  coordinate_future.GetCallback());
  feature()->Type(/*target_frame=*/nullptr, type_by_node_id, "default",
                  TypeAction::UNKNOWN_TYPE_MODE, false,
                  node_id_future.GetCallback());

  auto coordinate_result = coordinate_future.Get();
  EXPECT_FALSE(coordinate_result.IsOk());
  EXPECT_EQ(coordinate_result.code(), mojom::ActionResultCode::kFrameWentAway);
  auto node_id_result = node_id_future.Get();
  EXPECT_FALSE(node_id_result.IsOk());
  EXPECT_EQ(node_id_result.code(), mojom::ActionResultCode::kFrameWentAway);
}

TEST_F(TypeToolJavaScriptFeatureTest, TypeByCoordinate_Success) {
  MockTypeJsFunctions(
      /*mock_return_value=*/"{resultCode: 0, message: 'fake success!'}");
  ActionTarget action = CreateTargetWithCoordinates();
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->Type(GetMainFrame(feature()), action, "default",
                  TypeAction::UNKNOWN_TYPE_MODE, false, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.IsOk());
}

TEST_F(TypeToolJavaScriptFeatureTest, TypeByIdentifier_Success) {
  MockTypeJsFunctions(
      /*mock_return_value=*/"{resultCode: 0, message: 'fake success!'}");
  ActionTarget action = CreateTargetWithIdentifiers();
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->Type(GetMainFrame(feature()), action, "default",
                  TypeAction::UNKNOWN_TYPE_MODE, false, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.IsOk());
}

}  // namespace actor
