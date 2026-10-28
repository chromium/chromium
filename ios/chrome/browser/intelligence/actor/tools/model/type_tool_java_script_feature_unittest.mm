// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/type_tool_java_script_feature.h"

#import "base/strings/stringprintf.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_test_base.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/metrics.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

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

  // Mocks JavaScript function for typing to return the given result.
  void MockTypeJsFunctions(const std::string& mock_return_value) {
    MockJsFunction(feature(), "type_tool", "type", mock_return_value);
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

  void ExecuteType(base::WeakPtr<web::WebFrame> target_frame,
                   const ActionTarget& target,
                   const std::string& text,
                   optimization_guide::proto::TypeAction_TypeMode mode,
                   TypeToolJavaScriptFeature::TypeExecutionCallback callback) {
    feature()->Type(std::move(target_frame), target, text, mode,
                    std::move(callback));
  }

  void ExecuteSubmitWithEnterFallback(base::WeakPtr<web::WebFrame> target_frame,
                                      autofill::FieldRendererId field_id,
                                      base::OnceClosure callback) {
    feature()->SubmitWithEnterFallback(std::move(target_frame), field_id,
                                       std::move(callback));
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

// Tests that `Type()` extracts `fieldRendererId` when passed a
// `TypeExecutionCallback`, and returns a null `FieldRendererId` when missing,
// non-positive, out of `uint32_t` range, or when `target_frame` is null.
TEST_F(TypeToolJavaScriptFeatureTest, TypeExtractsFieldRendererId) {
  MockTypeJsFunctions(
      /*mock_return_value=*/
      "{resultCode: 0, message: 'fake success!', fieldRendererId: 42}");
  ActionTarget action = CreateTargetWithIdentifiers();
  base::test::TestFuture<ToolExecutionResult, autofill::FieldRendererId> future;

  ExecuteType(GetMainFrame(feature()), action, "default",
              TypeAction::UNKNOWN_TYPE_MODE, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().IsOk());
  EXPECT_EQ(future.Get<1>(), autofill::FieldRendererId(42));

  MockTypeJsFunctions(
      /*mock_return_value=*/"{resultCode: 0, message: 'fake success!'}");
  base::test::TestFuture<ToolExecutionResult, autofill::FieldRendererId>
      missing_id_future;
  ExecuteType(GetMainFrame(feature()), action, "default",
              TypeAction::UNKNOWN_TYPE_MODE, missing_id_future.GetCallback());
  EXPECT_TRUE(missing_id_future.Get<0>().IsOk());
  EXPECT_EQ(missing_id_future.Get<1>(), autofill::FieldRendererId());

  MockTypeJsFunctions(
      /*mock_return_value=*/
      "{resultCode: 0, message: 'fake success!', fieldRendererId: 0}");
  base::test::TestFuture<ToolExecutionResult, autofill::FieldRendererId>
      zero_id_future;
  ExecuteType(GetMainFrame(feature()), action, "default",
              TypeAction::UNKNOWN_TYPE_MODE, zero_id_future.GetCallback());
  EXPECT_TRUE(zero_id_future.Get<0>().IsOk());
  EXPECT_EQ(zero_id_future.Get<1>(), autofill::FieldRendererId());

  MockTypeJsFunctions(
      /*mock_return_value=*/
      "{resultCode: 0, message: 'fake success!', fieldRendererId: 5000000000}");
  base::test::TestFuture<ToolExecutionResult, autofill::FieldRendererId>
      overflow_id_future;
  ExecuteType(GetMainFrame(feature()), action, "default",
              TypeAction::UNKNOWN_TYPE_MODE, overflow_id_future.GetCallback());
  EXPECT_TRUE(overflow_id_future.Get<0>().IsOk());
  EXPECT_EQ(overflow_id_future.Get<1>(), autofill::FieldRendererId());

  base::test::TestFuture<ToolExecutionResult, autofill::FieldRendererId>
      null_frame_future;
  ExecuteType(/*target_frame=*/nullptr, action, "default",
              TypeAction::UNKNOWN_TYPE_MODE, null_frame_future.GetCallback());
  EXPECT_EQ(null_frame_future.Get<0>().code(),
            mojom::ActionResultCode::kFrameWentAway);
  EXPECT_EQ(null_frame_future.Get<1>(), autofill::FieldRendererId());
}

// Tests that the `Type()` overload taking `follow_by_enter` chains
// `SubmitWithEnterFallback()` only when typing succeeds, `follow_by_enter` is
// true, and `fieldRendererId` is valid.
TEST_F(TypeToolJavaScriptFeatureTest, TypeWithFollowByEnter) {
  base::HistogramTester histogram_tester;
  MockTypeJsFunctions(
      /*mock_return_value=*/
      "(window.__submittedFieldId = 0, "
      "{resultCode: 0, message: 'fake success!', fieldRendererId: 42})");
  MockJsFunction(feature(), "type_tool", "submitWithEnterFallback",
                 "(window.__submittedFieldId = arguments[0], "
                 "{resultCode: 0, message: 'fake fallback!'})");
  ActionTarget target = CreateTargetWithIdentifiers();

  base::test::TestFuture<ToolExecutionResult> enter_future;
  feature()->Type(GetMainFrame(feature()), target, "default",
                  TypeAction::UNKNOWN_TYPE_MODE, /*follow_by_enter=*/true,
                  enter_future.GetCallback());
  EXPECT_TRUE(enter_future.Get().IsOk());
  EXPECT_NSEQ(@42, web::test::ExecuteJavaScriptForFeatureAndReturnResult(
                       web_state(), @"window.__submittedFieldId", feature()));
  histogram_tester.ExpectUniqueSample(
      GetJavaScriptResultCodeHistogramName(
          ActorJavaScriptCall::kTypeSubmitWithEnterFallback),
      mojom::ActionResultCode::kOk, 1);
  histogram_tester.ExpectTotalCount(
      GetJavaScriptExecutionDurationHistogramName(
          ActorJavaScriptCall::kTypeSubmitWithEnterFallback),
      1);

  base::test::TestFuture<ToolExecutionResult> no_enter_future;
  feature()->Type(GetMainFrame(feature()), target, "default",
                  TypeAction::UNKNOWN_TYPE_MODE, /*follow_by_enter=*/false,
                  no_enter_future.GetCallback());
  EXPECT_TRUE(no_enter_future.Get().IsOk());
  EXPECT_NSEQ(@0, web::test::ExecuteJavaScriptForFeatureAndReturnResult(
                      web_state(), @"window.__submittedFieldId", feature()));

  MockTypeJsFunctions(
      /*mock_return_value=*/
      "(window.__submittedFieldId = 0, "
      "{resultCode: 0, message: 'fake success!', fieldRendererId: 0})");
  base::test::TestFuture<ToolExecutionResult> null_field_future;
  feature()->Type(GetMainFrame(feature()), target, "default",
                  TypeAction::UNKNOWN_TYPE_MODE, /*follow_by_enter=*/true,
                  null_field_future.GetCallback());
  EXPECT_TRUE(null_field_future.Get().IsOk());
  EXPECT_NSEQ(@0, web::test::ExecuteJavaScriptForFeatureAndReturnResult(
                      web_state(), @"window.__submittedFieldId", feature()));

  MockTypeJsFunctions(
      /*mock_return_value=*/
      "(window.__submittedFieldId = 0, "
      "{resultCode: 3, message: 'Not element', fieldRendererId: 42})");
  base::test::TestFuture<ToolExecutionResult> error_future;
  feature()->Type(GetMainFrame(feature()), target, "default",
                  TypeAction::UNKNOWN_TYPE_MODE, /*follow_by_enter=*/true,
                  error_future.GetCallback());
  EXPECT_FALSE(error_future.Get().IsOk());
  EXPECT_EQ(error_future.Get().code(),
            mojom::ActionResultCode::kTypeTargetNotElement);
  EXPECT_NSEQ(@0, web::test::ExecuteJavaScriptForFeatureAndReturnResult(
                      web_state(), @"window.__submittedFieldId", feature()));
  histogram_tester.ExpectTotalCount(
      GetJavaScriptResultCodeHistogramName(
          ActorJavaScriptCall::kTypeSubmitWithEnterFallback),
      1);
  histogram_tester.ExpectTotalCount(
      GetJavaScriptExecutionDurationHistogramName(
          ActorJavaScriptCall::kTypeSubmitWithEnterFallback),
      1);
}

// Tests that `SubmitWithEnterFallback()` invokes the JS fallback function,
// records UMA metrics, and runs its completion callback, or runs immediately
// without recording UMA if the frame or field ID is null.
TEST_F(TypeToolJavaScriptFeatureTest,
       SubmitWithEnterFallback_CallsJsAndRecordsMetrics) {
  base::HistogramTester histogram_tester;
  MockJsFunction(feature(), "type_tool", "submitWithEnterFallback",
                 "{resultCode: 0, message: 'fake fallback!'}");
  base::test::TestFuture<void> valid_future;
  ExecuteSubmitWithEnterFallback(GetMainFrame(feature()),
                                 autofill::FieldRendererId(42),
                                 valid_future.GetCallback());
  ASSERT_TRUE(valid_future.Wait());
  histogram_tester.ExpectUniqueSample(
      GetJavaScriptResultCodeHistogramName(
          ActorJavaScriptCall::kTypeSubmitWithEnterFallback),
      mojom::ActionResultCode::kOk, 1);
  histogram_tester.ExpectTotalCount(
      GetJavaScriptExecutionDurationHistogramName(
          ActorJavaScriptCall::kTypeSubmitWithEnterFallback),
      1);

  MockJsFunction(
      feature(), "type_tool", "submitWithEnterFallback",
      base::StringPrintf(
          "{resultCode: %d, message: 'Element missing'}",
          static_cast<int>(TypeToolResultCode::kTypeTargetNotElement)));
  base::test::TestFuture<void> error_future;
  ExecuteSubmitWithEnterFallback(GetMainFrame(feature()),
                                 autofill::FieldRendererId(42),
                                 error_future.GetCallback());
  ASSERT_TRUE(error_future.Wait());
  histogram_tester.ExpectBucketCount(
      GetJavaScriptResultCodeHistogramName(
          ActorJavaScriptCall::kTypeSubmitWithEnterFallback),
      mojom::ActionResultCode::kTypeTargetNotElement, 1);
  histogram_tester.ExpectTotalCount(
      GetJavaScriptExecutionDurationHistogramName(
          ActorJavaScriptCall::kTypeSubmitWithEnterFallback),
      2);

  base::test::TestFuture<void> null_frame_future;
  ExecuteSubmitWithEnterFallback(/*target_frame=*/nullptr,
                                 autofill::FieldRendererId(42),
                                 null_frame_future.GetCallback());
  ASSERT_TRUE(null_frame_future.Wait());

  base::test::TestFuture<void> null_field_future;
  ExecuteSubmitWithEnterFallback(GetMainFrame(feature()),
                                 autofill::FieldRendererId(),
                                 null_field_future.GetCallback());
  ASSERT_TRUE(null_field_future.Wait());
  histogram_tester.ExpectTotalCount(
      GetJavaScriptResultCodeHistogramName(
          ActorJavaScriptCall::kTypeSubmitWithEnterFallback),
      2);
  histogram_tester.ExpectTotalCount(
      GetJavaScriptExecutionDurationHistogramName(
          ActorJavaScriptCall::kTypeSubmitWithEnterFallback),
      2);
}

}  // namespace actor
