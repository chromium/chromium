// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/scroll_tool_java_script_feature.h"

#import "base/strings/stringprintf.h"
#import "base/test/bind.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_test_base.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "testing/gtest/include/gtest/gtest.h"

using optimization_guide::proto::ScrollAction;
using optimization_guide::proto::ScrollToAction;

namespace actor {

class ScrollToolJavaScriptFeatureTest
    : public ActorToolJavaScriptFeatureTestBase {
 protected:
  ScrollToolJavaScriptFeatureTest() : ActorToolJavaScriptFeatureTestBase() {}

  void SetUp() override { ActorToolJavaScriptFeatureTestBase::SetUp(); }

  ScrollToolJavaScriptFeature* feature() {
    return ScrollToolJavaScriptFeature::GetInstance();
  }

  // Mocks both JavaScript functions for scrolling to return the given result.
  void MockScrollJsFunctions(const std::string& mock_return_value) {
    MockJsFunction(feature(), "scroll_tool", "scrollByCoordinate",
                   mock_return_value);
    MockJsFunction(feature(), "scroll_tool", "scrollByNodeId",
                   mock_return_value);
  }

  ActionTarget CreateTargetWithCoordinates() {
    optimization_guide::proto::ActionTarget target;
    // Use arbitrary constants since the JS calls are mocked.
    target.mutable_coordinate()->set_x(1);
    target.mutable_coordinate()->set_y(2);
    target.mutable_coordinate()->set_pixel_type(
        optimization_guide::proto::Coordinate::PIXEL_TYPE_UNSPECIFIED);
    return ActionTarget::FromProto(target);
  }

  ActionTarget CreateTargetWithIdentifiers() {
    optimization_guide::proto::ActionTarget target;
    // Use arbitrary constants since the JS calls are mocked.
    target.set_content_node_id(0);
    target.mutable_document_identifier()->set_serialized_token("token");
    return ActionTarget::FromProto(target);
  }
};

TEST_F(ScrollToolJavaScriptFeatureTest, JsReturnsNonDict) {
  MockScrollJsFunctions(/*mock_return_value=*/"'unexpected type'");
  ActionTarget scroll_by_coordinate = CreateTargetWithCoordinates();
  ActionTarget scroll_to_by_node_id = CreateTargetWithIdentifiers();
  base::test::TestFuture<ToolExecutionResult> scroll_future;
  base::test::TestFuture<ToolExecutionResult> scroll_to_future;

  feature()->Scroll(GetMainFrame(feature()), scroll_by_coordinate,
                    optimization_guide::proto::ScrollAction::DOWN, 123.0,
                    scroll_future.GetCallback());
  feature()->ScrollTo(GetMainFrame(feature()), scroll_to_by_node_id,
                      scroll_to_future.GetCallback());

  auto scroll_result = scroll_future.Get();
  EXPECT_FALSE(scroll_result.IsOk());
  EXPECT_EQ(scroll_result.internal_code().value(),
            InternalToolErrorCode::kJavascriptFeatureGotInvalidResult);

  auto scroll_to_result = scroll_to_future.Get();
  EXPECT_FALSE(scroll_to_result.IsOk());
  EXPECT_EQ(scroll_to_result.internal_code().value(),
            InternalToolErrorCode::kJavascriptFeatureGotInvalidResult);
}

TEST_F(ScrollToolJavaScriptFeatureTest, JsReturnsError) {
  auto js_code = ScrollToolResultCode::kScrollTargetNotUserScrollable;
  auto expected_code = mojom::ActionResultCode::kScrollTargetNotUserScrollable;
  MockScrollJsFunctions(
      /*mock_return_value=*/base::StringPrintf(
          "{resultCode: %d, message: 'Custom JS Error'}", js_code));
  ActionTarget scroll_by_coordinate = CreateTargetWithCoordinates();
  ActionTarget scroll_to_by_node_id = CreateTargetWithIdentifiers();
  base::test::TestFuture<ToolExecutionResult> scroll_future;
  base::test::TestFuture<ToolExecutionResult> scroll_to_future;

  feature()->Scroll(GetMainFrame(feature()), scroll_by_coordinate,
                    optimization_guide::proto::ScrollAction::DOWN, 123.0,
                    scroll_future.GetCallback());
  feature()->ScrollTo(GetMainFrame(feature()), scroll_to_by_node_id,
                      scroll_to_future.GetCallback());

  auto scroll_result = scroll_future.Get();
  EXPECT_FALSE(scroll_result.IsOk());
  EXPECT_EQ(scroll_result.code(), expected_code);
  EXPECT_EQ(GetToolExecutionResultMessage(scroll_result), "Custom JS Error");

  auto scroll_to_result = scroll_to_future.Get();
  EXPECT_FALSE(scroll_to_result.IsOk());
  EXPECT_EQ(scroll_to_result.code(), expected_code);
  EXPECT_EQ(GetToolExecutionResultMessage(scroll_to_result), "Custom JS Error");
}

TEST_F(ScrollToolJavaScriptFeatureTest, WebFrameInvalidated) {
  MockScrollJsFunctions(
      /*mock_return_value=*/"{resultCode: 0, message: 'fake success!'}");
  ActionTarget scroll_by_coordinate = CreateTargetWithCoordinates();
  ActionTarget scroll_to_by_node_id = CreateTargetWithIdentifiers();
  base::test::TestFuture<ToolExecutionResult> scroll_future;
  base::test::TestFuture<ToolExecutionResult> scroll_to_future;

  feature()->Scroll(/*target_frame=*/nullptr, scroll_by_coordinate,
                    optimization_guide::proto::ScrollAction::DOWN, 123.0,
                    scroll_future.GetCallback());
  feature()->ScrollTo(/*target_frame=*/nullptr, scroll_to_by_node_id,
                      scroll_to_future.GetCallback());

  auto scroll_result = scroll_future.Get();
  EXPECT_FALSE(scroll_result.IsOk());
  EXPECT_EQ(scroll_result.code(), mojom::ActionResultCode::kFrameWentAway);
  auto scroll_to_result = scroll_to_future.Get();
  EXPECT_FALSE(scroll_to_result.IsOk());
  EXPECT_EQ(scroll_to_result.code(), mojom::ActionResultCode::kFrameWentAway);
}

TEST_F(ScrollToolJavaScriptFeatureTest, Scroll_ByCoordinate_Success) {
  MockScrollJsFunctions(
      /*mock_return_value=*/"{resultCode: 0, message: 'fake success!'}");
  ActionTarget action = CreateTargetWithCoordinates();
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->Scroll(GetMainFrame(feature()), action,
                    optimization_guide::proto::ScrollAction::DOWN, 123.0,
                    future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.IsOk());
}

TEST_F(ScrollToolJavaScriptFeatureTest, Scroll_ByIdentifier_Success) {
  MockScrollJsFunctions(
      /*mock_return_value=*/"{resultCode: 0, message: 'fake success!'}");
  ActionTarget action = CreateTargetWithIdentifiers();
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->Scroll(GetMainFrame(feature()), action,
                    optimization_guide::proto::ScrollAction::DOWN, 123.0,
                    future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.IsOk());
}

TEST_F(ScrollToolJavaScriptFeatureTest, ScrollTo_ByCoordinate_Success) {
  MockScrollJsFunctions(
      /*mock_return_value=*/"{resultCode: 0, message: 'fake success!'}");
  ActionTarget action = CreateTargetWithCoordinates();
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->ScrollTo(GetMainFrame(feature()), action, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.IsOk());
}

TEST_F(ScrollToolJavaScriptFeatureTest, ScrollTo_ByIdentifier_Success) {
  MockScrollJsFunctions(
      /*mock_return_value=*/"{resultCode: 0, message: 'fake success!'}");
  ActionTarget action = CreateTargetWithIdentifiers();
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->ScrollTo(GetMainFrame(feature()), action, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.IsOk());
}

}  // namespace actor
