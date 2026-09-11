// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/drag_and_release_tool_java_script_feature.h"

#import <tuple>

#import "base/strings/stringprintf.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_test_base.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace actor {

class DragAndReleaseToolJavaScriptFeatureTest
    : public ActorToolJavaScriptFeatureTestBase {
 protected:
  DragAndReleaseToolJavaScriptFeatureTest() = default;

  DragAndReleaseToolJavaScriptFeature* feature() {
    return DragAndReleaseToolJavaScriptFeature::GetInstance();
  }

  // Mocks the drag and release JS function to return the given result.
  void MockDragAndReleaseJsFunction(const std::string& mock_return_value) {
    MockJsFunction(feature(), "drag_and_release_tool", "dragAndRelease",
                   mock_return_value);
  }

  ToolExecutionResult ExecuteDragAndRelease(
      base::WeakPtr<web::WebFrame> target_frame,
      const ActionTarget& from_target,
      const ActionTarget& to_target) {
    base::test::TestFuture<ToolExecutionResult> future;
    feature()->DragAndRelease(target_frame, from_target, to_target,
                              future.GetCallback());
    return future.Get();
  }
};

TEST_F(DragAndReleaseToolJavaScriptFeatureTest, JsReturnsNonDict_Errors) {
  MockDragAndReleaseJsFunction(/*mock_return_value=*/"'unexpected type'");
  ActionTarget from_target = CreateTargetWithCoordinates();
  ActionTarget to_target = CreateTargetWithCoordinates();

  auto result =
      ExecuteDragAndRelease(GetMainFrame(feature()), from_target, to_target);

  EXPECT_FALSE(result.IsOk());
  ASSERT_TRUE(result.internal_code().has_value());
  EXPECT_EQ(result.internal_code().value(),
            InternalToolErrorCode::kJavascriptFeatureGotInvalidResult);
}

TEST_F(DragAndReleaseToolJavaScriptFeatureTest, InvalidWebFrame_Errors) {
  ActionTarget from_target = CreateTargetWithCoordinates();
  ActionTarget to_target = CreateTargetWithCoordinates();

  auto result = ExecuteDragAndRelease(base::WeakPtr<web::WebFrame>(),
                                      from_target, to_target);

  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kFrameWentAway);
}

struct DragAndReleaseErrorCodeTestCase {
  DragAndReleaseToolResultCode js_code;
  mojom::ActionResultCode expected_code;
  const char* test_name;
};

class DragAndReleaseToolJavaScriptFeatureErrorCodeTest
    : public DragAndReleaseToolJavaScriptFeatureTest,
      public testing::WithParamInterface<DragAndReleaseErrorCodeTestCase> {};

TEST_P(DragAndReleaseToolJavaScriptFeatureErrorCodeTest, ErrorCodeMapping) {
  MockDragAndReleaseJsFunction(
      /*mock_return_value=*/base::StringPrintf(
          "{resultCode: %d, message: 'Custom message'}",
          static_cast<int>(GetParam().js_code)));
  ActionTarget from_target = CreateTargetWithCoordinates();
  ActionTarget to_target = CreateTargetWithCoordinates();

  auto result =
      ExecuteDragAndRelease(GetMainFrame(feature()), from_target, to_target);

  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), GetParam().expected_code);
  ASSERT_TRUE(result.message().has_value());
  EXPECT_EQ(result.message().value(), "Custom message");
}

INSTANTIATE_TEST_SUITE_P(
    ,
    DragAndReleaseToolJavaScriptFeatureErrorCodeTest,
    testing::Values(
        DragAndReleaseErrorCodeTestCase{
            DragAndReleaseToolResultCode::kFromCoordinatesOutOfBounds,
            mojom::ActionResultCode::kDragAndReleaseFromOffscreen,
            "FromCoordinatesOutOfBounds"},
        DragAndReleaseErrorCodeTestCase{
            DragAndReleaseToolResultCode::kToCoordinatesOutOfBounds,
            mojom::ActionResultCode::kDragAndReleaseToOffscreen,
            "ToCoordinatesOutOfBounds"},
        DragAndReleaseErrorCodeTestCase{
            DragAndReleaseToolResultCode::kFromInvalidDomNodeId,
            mojom::ActionResultCode::kInvalidDomNodeId, "FromInvalidDomNodeId"},
        DragAndReleaseErrorCodeTestCase{
            DragAndReleaseToolResultCode::kToInvalidDomNodeId,
            mojom::ActionResultCode::kInvalidDomNodeId, "ToInvalidDomNodeId"},
        DragAndReleaseErrorCodeTestCase{
            DragAndReleaseToolResultCode::kFromElementDisabled,
            mojom::ActionResultCode::kElementDisabled, "FromElementDisabled"},
        DragAndReleaseErrorCodeTestCase{
            DragAndReleaseToolResultCode::kToElementDisabled,
            mojom::ActionResultCode::kElementDisabled, "ToElementDisabled"},
        DragAndReleaseErrorCodeTestCase{
            DragAndReleaseToolResultCode::kDragSuppressed,
            mojom::ActionResultCode::kDragAndReleaseDownSuppressed,
            "DragSuppressed"}),
    [](const testing::TestParamInfo<DragAndReleaseErrorCodeTestCase>& info) {
      return info.param.test_name;
    });

enum class TargetType { kCoordinate, kNodeId };

class DragAndReleaseToolJavaScriptFeatureSuccessTest
    : public DragAndReleaseToolJavaScriptFeatureTest,
      public testing::WithParamInterface<std::tuple<TargetType, TargetType>> {};

TEST_P(DragAndReleaseToolJavaScriptFeatureSuccessTest, Execute) {
  MockDragAndReleaseJsFunction(
      /*mock_return_value=*/"{resultCode: 0, message: 'Success'}");
  const auto& [from_type, to_type] = GetParam();
  ActionTarget from_target = (from_type == TargetType::kCoordinate)
                                 ? CreateTargetWithCoordinates()
                                 : CreateTargetWithNodeId();
  ActionTarget to_target = (to_type == TargetType::kCoordinate)
                               ? CreateTargetWithCoordinates()
                               : CreateTargetWithNodeId();

  auto result =
      ExecuteDragAndRelease(GetMainFrame(feature()), from_target, to_target);

  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kOk);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    DragAndReleaseToolJavaScriptFeatureSuccessTest,
    testing::Combine(
        testing::Values(TargetType::kCoordinate, TargetType::kNodeId),
        testing::Values(TargetType::kCoordinate, TargetType::kNodeId)),
    [](const testing::TestParamInfo<
        DragAndReleaseToolJavaScriptFeatureSuccessTest::ParamType>& info) {
      TargetType from_type = std::get<0>(info.param);
      TargetType to_type = std::get<1>(info.param);
      std::string from =
          from_type == TargetType::kCoordinate ? "Coordinate" : "Node";
      std::string to =
          to_type == TargetType::kCoordinate ? "Coordinate" : "Node";
      return from + "To" + to;
    });

}  // namespace actor
