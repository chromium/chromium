// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/select_tool_java_script_feature.h"

#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_test_base.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "testing/gtest/include/gtest/gtest.h"

using optimization_guide::proto::SelectAction;

namespace actor {

class SelectToolJavaScriptFeatureTest
    : public ActorToolJavaScriptFeatureTestBase {
 protected:
  SelectToolJavaScriptFeatureTest() : ActorToolJavaScriptFeatureTestBase() {}

  void SetUp() override { ActorToolJavaScriptFeatureTestBase::SetUp(); }

  SelectToolJavaScriptFeature* feature() {
    return SelectToolJavaScriptFeature::GetInstance();
  }

  // Mocks both JavaScript functions for selecting to return the given result.
  void MockSelectJsFunctions(const std::string& mock_return_value) {
    MockJsFunction(feature(), "select_tool", "selectByCoordinate",
                   mock_return_value);
    MockJsFunction(feature(), "select_tool", "selectByNodeId",
                   mock_return_value);
  }

  SelectAction CreateSelectActionWithCoordinates() {
    SelectAction action;
    // Use arbitrary values since the JS function is mocked.
    action.mutable_target()->mutable_coordinate()->set_x(1);
    action.mutable_target()->mutable_coordinate()->set_y(2);
    action.mutable_target()->mutable_coordinate()->set_pixel_type(
        optimization_guide::proto::Coordinate::PIXEL_TYPE_UNSPECIFIED);
    action.set_value("selected_value");
    return action;
  }

  SelectAction CreateSelectActionWithNodeId() {
    SelectAction action;
    // Use arbitrary values since the JS function is mocked.
    action.mutable_target()->set_content_node_id(123);
    action.mutable_target()
        ->mutable_document_identifier()
        ->set_serialized_token("doc_id");
    action.set_value("selected_value");
    return action;
  }
};

TEST_F(SelectToolJavaScriptFeatureTest, JsReturnsNonDict) {
  MockSelectJsFunctions(/*mock_return_value=*/"'unexpected type'");
  SelectAction select_by_coordinate = CreateSelectActionWithCoordinates();
  SelectAction select_by_node_id = CreateSelectActionWithNodeId();
  base::test::TestFuture<ToolExecutionResult> coordinate_future;
  base::test::TestFuture<ToolExecutionResult> node_id_future;

  feature()->Select(GetMainFrame(feature()), select_by_coordinate,
                    coordinate_future.GetCallback());
  feature()->Select(GetMainFrame(feature()), select_by_node_id,
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

TEST_F(SelectToolJavaScriptFeatureTest, JsReturnsError) {
  MockSelectJsFunctions(
      /*mock_return_value=*/"{success: false, message: 'Custom JS Error'}");
  SelectAction select_by_coordinate = CreateSelectActionWithCoordinates();
  SelectAction select_by_node_id = CreateSelectActionWithNodeId();
  base::test::TestFuture<ToolExecutionResult> coordinate_future;
  base::test::TestFuture<ToolExecutionResult> node_id_future;

  feature()->Select(GetMainFrame(feature()), select_by_coordinate,
                    coordinate_future.GetCallback());
  feature()->Select(GetMainFrame(feature()), select_by_node_id,
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

TEST_F(SelectToolJavaScriptFeatureTest, InvalidatedWebFrame) {
  SelectAction select_by_coordinate = CreateSelectActionWithCoordinates();
  SelectAction select_by_node_id = CreateSelectActionWithNodeId();
  base::test::TestFuture<ToolExecutionResult> coordinate_future;
  base::test::TestFuture<ToolExecutionResult> node_id_future;

  feature()->Select(/*target_frame=*/nullptr, select_by_coordinate,
                    coordinate_future.GetCallback());
  feature()->Select(/*target_frame=*/nullptr, select_by_node_id,
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

TEST_F(SelectToolJavaScriptFeatureTest, JsReturnsErrorWithoutMessage) {
  MockSelectJsFunctions(/*mock_return_value=*/"{success: false}");
  SelectAction select_by_coordinate = CreateSelectActionWithCoordinates();
  SelectAction select_by_node_id = CreateSelectActionWithNodeId();
  base::test::TestFuture<ToolExecutionResult> coordinate_future;
  base::test::TestFuture<ToolExecutionResult> node_id_future;

  feature()->Select(GetMainFrame(feature()), select_by_coordinate,
                    coordinate_future.GetCallback());
  feature()->Select(GetMainFrame(feature()), select_by_node_id,
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

TEST_F(SelectToolJavaScriptFeatureTest, SelectByCoordinate_Success) {
  MockSelectJsFunctions(
      /*mock_return_value=*/"{success: true, message: 'fake success!'}");
  SelectAction action = CreateSelectActionWithCoordinates();
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->Select(GetMainFrame(feature()), action, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.IsOk());
}

TEST_F(SelectToolJavaScriptFeatureTest, SelectByNodeId_Success) {
  MockSelectJsFunctions(
      /*mock_return_value=*/"{success: true, message: 'fake success!'}");
  SelectAction action = CreateSelectActionWithNodeId();
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->Select(GetMainFrame(feature()), action, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.IsOk());
}

}  // namespace actor
