// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/scroll_tool_java_script_feature.h"

#import "base/test/bind.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_test_base.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"
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

  ScrollAction CreateScrollActionWithCoordinates() {
    ScrollAction action;
    // Use arbitrary constants since the JS calls are mocked.
    action.mutable_target()->mutable_coordinate()->set_x(1);
    action.mutable_target()->mutable_coordinate()->set_y(2);
    action.mutable_target()->mutable_coordinate()->set_pixel_type(
        optimization_guide::proto::Coordinate::PIXEL_TYPE_UNSPECIFIED);
    action.set_direction(optimization_guide::proto::ScrollAction::DOWN);
    action.set_distance(123.0);
    return action;
  }

  ScrollAction CreateScrollActionWithIdentifiers() {
    ScrollAction action;
    // Use arbitrary constants since the JS calls are mocked.
    action.mutable_target()->set_content_node_id(0);
    action.mutable_target()
        ->mutable_document_identifier()
        ->set_serialized_token("token");
    action.set_direction(optimization_guide::proto::ScrollAction::DOWN);
    action.set_distance(123.0);
    return action;
  }

  ScrollToAction CreateScrollToActionWithCoordinates() {
    ScrollToAction action;
    // Use arbitrary constants since the JS calls are mocked.
    action.mutable_target()->mutable_coordinate()->set_x(1);
    action.mutable_target()->mutable_coordinate()->set_y(2);
    action.mutable_target()->mutable_coordinate()->set_pixel_type(
        optimization_guide::proto::Coordinate::PIXEL_TYPE_UNSPECIFIED);
    return action;
  }

  ScrollToAction CreateScrollToActionWithIdentifiers() {
    // Use arbitrary constants since the JS calls are mocked.
    ScrollToAction action;
    action.mutable_target()->set_content_node_id(0);
    action.mutable_target()
        ->mutable_document_identifier()
        ->set_serialized_token("token");
    return action;
  }
};

TEST_F(ScrollToolJavaScriptFeatureTest, JsReturnsNonDict) {
  MockScrollJsFunctions(/*mock_return_value=*/"'unexpected type'");
  ScrollAction scroll_by_coordinate = CreateScrollActionWithCoordinates();
  ScrollToAction scroll_to_by_node_id = CreateScrollToActionWithIdentifiers();
  base::test::TestFuture<ToolExecutionResult> scroll_future;
  base::test::TestFuture<ToolExecutionResult> scroll_to_future;

  feature()->Scroll(GetMainFrame(feature()), scroll_by_coordinate,
                    scroll_future.GetCallback());
  feature()->ScrollTo(GetMainFrame(feature()), scroll_to_by_node_id,
                      scroll_to_future.GetCallback());

  auto scroll_result = scroll_future.Get();
  EXPECT_FALSE(scroll_result.has_value());
  EXPECT_EQ(scroll_result.error().code,
            ActorToolErrorCode::kJavascriptFeatureGotInvalidResult);

  auto scroll_to_result = scroll_to_future.Get();
  EXPECT_FALSE(scroll_to_result.has_value());
  EXPECT_EQ(scroll_to_result.error().code,
            ActorToolErrorCode::kJavascriptFeatureGotInvalidResult);
}

TEST_F(ScrollToolJavaScriptFeatureTest, JsReturnsError) {
  MockScrollJsFunctions(
      /*mock_return_value=*/"{success: false, message: 'Custom JS Error'}");
  ScrollAction scroll_by_coordinate = CreateScrollActionWithCoordinates();
  ScrollToAction scroll_to_by_node_id = CreateScrollToActionWithIdentifiers();
  base::test::TestFuture<ToolExecutionResult> scroll_future;
  base::test::TestFuture<ToolExecutionResult> scroll_to_future;

  feature()->Scroll(GetMainFrame(feature()), scroll_by_coordinate,
                    scroll_future.GetCallback());
  feature()->ScrollTo(GetMainFrame(feature()), scroll_to_by_node_id,
                      scroll_to_future.GetCallback());

  auto scroll_result = scroll_future.Get();
  EXPECT_FALSE(scroll_result.has_value());
  EXPECT_EQ(scroll_result.error().code,
            ActorToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution);
  EXPECT_EQ(scroll_result.error().message, "Custom JS Error");

  auto scroll_to_result = scroll_to_future.Get();
  EXPECT_FALSE(scroll_to_result.has_value());
  EXPECT_EQ(scroll_to_result.error().code,
            ActorToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution);
  EXPECT_EQ(scroll_to_result.error().message, "Custom JS Error");
}

TEST_F(ScrollToolJavaScriptFeatureTest, WebFrameInvalidated) {
  MockScrollJsFunctions(
      /*mock_return_value=*/"{success: true, message: 'fake success!'}");
  ScrollAction scroll_by_coordinate = CreateScrollActionWithCoordinates();
  ScrollToAction scroll_to_by_node_id = CreateScrollToActionWithIdentifiers();
  base::test::TestFuture<ToolExecutionResult> scroll_future;
  base::test::TestFuture<ToolExecutionResult> scroll_to_future;

  feature()->Scroll(nullptr, scroll_by_coordinate, scroll_future.GetCallback());
  feature()->ScrollTo(nullptr, scroll_to_by_node_id,
                      scroll_to_future.GetCallback());

  auto scroll_result = scroll_future.Get();
  EXPECT_FALSE(scroll_result.has_value());
  EXPECT_EQ(scroll_result.error().code,
            ActorToolErrorCode::kActorTargetWebFrameInvalidated);
  auto scroll_to_result = scroll_to_future.Get();
  EXPECT_FALSE(scroll_to_result.has_value());
  EXPECT_EQ(scroll_to_result.error().code,
            ActorToolErrorCode::kActorTargetWebFrameInvalidated);
}

TEST_F(ScrollToolJavaScriptFeatureTest, Scroll_ByCoordinate_Success) {
  MockScrollJsFunctions(
      /*mock_return_value=*/"{success: true, message: 'fake success!'}");
  ScrollAction action = CreateScrollActionWithCoordinates();
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->Scroll(GetMainFrame(feature()), action, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
}

TEST_F(ScrollToolJavaScriptFeatureTest, Scroll_ByIdentifier_Success) {
  MockScrollJsFunctions(
      /*mock_return_value=*/"{success: true, message: 'fake success!'}");
  ScrollAction action = CreateScrollActionWithIdentifiers();
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->Scroll(GetMainFrame(feature()), action, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
}

TEST_F(ScrollToolJavaScriptFeatureTest, ScrollTo_ByCoordinate_Success) {
  MockScrollJsFunctions(
      /*mock_return_value=*/"{success: true, message: 'fake success!'}");
  ScrollToAction action = CreateScrollToActionWithCoordinates();
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->ScrollTo(GetMainFrame(feature()), action, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
}

TEST_F(ScrollToolJavaScriptFeatureTest, ScrollTo_ByIdentifier_Success) {
  MockScrollJsFunctions(
      /*mock_return_value=*/"{success: true, message: 'fake success!'}");
  ScrollToAction action = CreateScrollToActionWithIdentifiers();
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->ScrollTo(GetMainFrame(feature()), action, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
}

}  // namespace actor
